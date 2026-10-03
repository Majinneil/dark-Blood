# Blender (headless):  blender -b --python Tools/Blender/split_alpha.py -- <in.png> <out_dir> <prefix>
#
# Splits an RGBA base color texture for the foliage master (M_DB_Foliage_Master): <prefix>_alpha.png (grey opacity mask)
# and a flat <prefix>_arm.png (AO 1, roughness 0.9, metal 0). Used for models whose alpha cards the imported glTF material
# does not cut out (e.g. the thatch and snow cards of the shirakawago house).
import bpy
import os
import sys

args = sys.argv[sys.argv.index("--") + 1:]
source, out_dir, prefix = args[0], args[1], args[2]
os.makedirs(out_dir, exist_ok=True)
image = bpy.data.images.load(source)
width, height = image.size
pixels = list(image.pixels)

def save(name, values):
    out = bpy.data.images.new(name, width, height, alpha=False)
    out.pixels = values
    out.filepath_raw = os.path.join(out_dir, name + ".png")
    out.file_format = "PNG"
    out.save()
    print("DBSPLIT wrote", out.filepath_raw)

alpha = pixels[3::4]
save(prefix + "_alpha", [c for a in alpha for c in (a, a, a, 1.0)])
save(prefix + "_arm", [1.0, 0.9, 0.0, 1.0] * (width * height))
