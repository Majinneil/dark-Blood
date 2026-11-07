# Run with Blender 5.x in background mode:
#   blender -b --python Tools/Art/blender_refine_hyper3d.py -- <Name> [<Name> ...]
#
# Sharpens the textures of a rigged Hyper3D body (SourceArt/Hyper3D/<Name>/export, from Tools/UE58/blender_rig_hyper3d.py).
# Rodin's free "Low" models carry only coarse forms in their normal map while the base color holds the fine work
# (rivets, embroidery, scratches). From the base color's fine structure this script derives
#   - a detail normal, blended into the normal map (reoriented normal mapping) -> T_<Name>_Normal.png
#   - a cavity mask that darkens crevices in the base color and roughens them a little -> T_<Name>_BaseColor.png / _ORM.png
# The first run keeps Rodin's maps as T_<Name>_<Usage>_rodin.png and every run starts from those, so it can be repeated.
# Normals stay in glTF (OpenGL) convention; the importer flips green for Unreal.
import os
import shutil
import sys

import bpy
import numpy as np

PROJECT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
NAMES = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
DETAIL_STRENGTH = 2.2      # slope of the detail normal per unit of fine luminance
CAVITY_DARKEN = 0.35       # how much the deepest crevices darken the base color
CAVITY_ROUGHEN = 0.12      # added roughness in crevices
BLUR_RADIUS = 5            # pixels: structure finer than this counts as detail (2K maps)


def log(message):
    print("DBREFINE " + message)


def load(path, linear):
    image = bpy.data.images.load(path)
    image.colorspace_settings.name = "Non-Color" if linear else "sRGB"
    width, height = image.size
    pixels = np.array(image.pixels[:], dtype=np.float32).reshape(height, width, 4)
    bpy.data.images.remove(image)
    return pixels


def save(path, pixels, linear):
    height, width = pixels.shape[:2]
    image = bpy.data.images.new(os.path.basename(path), width, height, alpha=True, float_buffer=False)
    image.colorspace_settings.name = "Non-Color" if linear else "sRGB"
    image.pixels = np.clip(pixels, 0.0, 1.0).ravel()
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()
    bpy.data.images.remove(image)


def box_blur(values, radius):
    """Separable box blur by cumulative sums, two passes (close to a gaussian), edges clamped."""
    for _ in range(2):
        for axis in (0, 1):
            padded = np.pad(values, [(radius + 1, radius)] if values.ndim == 1 else
                            [((radius + 1, radius) if a == axis else (0, 0)) for a in range(values.ndim)], mode="edge")
            summed = np.cumsum(padded, axis=axis)
            upper = np.take(summed, range(2 * radius + 1, summed.shape[axis]), axis=axis)
            lower = np.take(summed, range(0, summed.shape[axis] - 2 * radius - 1), axis=axis)
            values = (upper - lower) / (2 * radius + 1)
    return values


def refine(name):
    export = os.path.join(PROJECT, "SourceArt", "Hyper3D", name, "export")
    paths = {usage: os.path.join(export, "T_%s_%s.png" % (name, usage)) for usage in ("BaseColor", "Normal", "ORM")}
    if not all(os.path.isfile(p) for p in paths.values()):
        log("%s: textures missing, skipped" % name)
        return
    originals = {}
    for usage, path in paths.items():
        original = path[:-4] + "_rodin.png"
        if not os.path.isfile(original):
            shutil.copyfile(path, original)
        originals[usage] = original
    base = load(originals["BaseColor"], linear=False)
    normal = load(originals["Normal"], linear=True)
    orm = load(originals["ORM"], linear=True)

    # Fine structure of the base color: luminance minus its blurred self (texels are in Blender's bottom-up order).
    luminance = base[..., 0] * 0.2126 + base[..., 1] * 0.7152 + base[..., 2] * 0.0722
    fine = luminance - box_blur(luminance, BLUR_RADIUS)
    scale = max(np.percentile(np.abs(fine), 98), 1e-4)
    fine = np.clip(fine / scale, -1.0, 1.0)

    # Detail normal from the slope of the fine structure (bright = raised), OpenGL convention like the glTF maps.
    slope_y, slope_x = np.gradient(fine)
    detail = np.stack([-slope_x * DETAIL_STRENGTH, -slope_y * DETAIL_STRENGTH, np.ones_like(fine)], axis=-1)
    detail /= np.linalg.norm(detail, axis=-1, keepdims=True)
    # Reoriented normal mapping: the detail bends around Rodin's coarse normal instead of averaging it away.
    coarse = normal[..., :3] * 2.0 - 1.0
    t = coarse + np.array([0.0, 0.0, 1.0], dtype=np.float32)
    u = detail * np.array([-1.0, -1.0, 1.0], dtype=np.float32)
    blended = t * (np.sum(t * u, axis=-1, keepdims=True) / np.maximum(t[..., 2:3], 1e-4)) - u
    blended /= np.linalg.norm(blended, axis=-1, keepdims=True)
    normal[..., :3] = blended * 0.5 + 0.5

    # Cavities (darker than their surroundings) darken and roughen; raised edges stay as they are.
    cavity = np.clip(-fine, 0.0, 1.0)[..., None]
    base[..., :3] *= 1.0 - CAVITY_DARKEN * cavity
    orm[..., 1] = np.clip(orm[..., 1] + CAVITY_ROUGHEN * cavity[..., 0], 0.0, 1.0)

    save(paths["BaseColor"], base, linear=False)
    save(paths["Normal"], normal, linear=True)
    save(paths["ORM"], orm, linear=True)
    log("%s: refined %dx%d (detail %.1f, cavity %.2f)" % (name, base.shape[1], base.shape[0], DETAIL_STRENGTH, CAVITY_DARKEN))


for character in NAMES:
    refine(character)
