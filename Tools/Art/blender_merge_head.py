# Run with Blender 5.x in background mode:
#   blender -b --python Tools/Art/blender_merge_head.py -- <body.glb> <head.glb> <out.glb>
#
# Puts a separately generated, higher-detail head (Hyper3D Rodin from the head reference sheet) onto a full-body model:
# the body's own head is cut off under the jaw inside a cylinder about the head axis (the scarf stays), the new head is
# scaled so nose-to-crown matches the old head, moved so the nose tips meet, and its neck runs a few centimetres down into
# the collar (no seam visible). Both keep their own UVs and textures as separate materials ("Body", "Head"); the result
# is a GLB for Tools/UE58/blender_rig_hyper3d.py.
import os
import sys

import bpy
import bmesh
import numpy as np
from mathutils import Matrix, Vector, geometry
from mathutils.bvhtree import BVHTree

args = sys.argv[sys.argv.index("--") + 1:]
BODY, HEAD, OUT = (os.path.abspath(a) for a in args[:3])


def log(message):
    print("DBHEAD " + message, flush=True)


def import_glb(path, name):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    meshes = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    for obj in meshes:
        obj.data.transform(obj.matrix_world)
        obj.matrix_world = Matrix.Identity(4)
        obj.parent = None
    for obj in [o for o in bpy.data.objects if o not in before and o.type != "MESH"]:
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = name
    for mat in obj.data.materials:
        if mat:
            mat.name = name
    return obj


bpy.ops.wm.read_factory_settings(use_empty=True)
body = import_glb(BODY, "Body")
head = import_glb(HEAD, "Head")

# Alignment by the head's outline (robust where faces, beards and hair defeat landmark searches):
#   body: the head starts where the section narrows from the scarf/shoulders to head width (scanning up the top 20 %);
#   bust: the neck is the narrowest section in the lower 40 % (below it the bust widens into shoulders).
# Scale = old head height (neck to crown) / new head height; the head regions' bounding-box centres meet in X and Y,
# the necks meet in Z.
def sections(obj, low, high, steps=60):
    verts = [v.co for v in obj.data.vertices]
    z0, z1 = min(v.z for v in verts), max(v.z for v in verts)
    out = []
    for i in range(steps + 1):
        z = z0 + (z1 - z0) * (low + (high - low) * i / steps)
        ring = [v for v in verts if abs(v.z - z) < (z1 - z0) * 0.005]
        if len(ring) >= 12:
            out.append((z, max(v.x for v in ring) - min(v.x for v in ring)))
    return out, z1, verts


def region_centre(verts, z_min, z_max):
    """X: middle of the head region; Y: its front (face) - a bun or long hair at the back must not shift the face."""
    region = [v for v in verts if z_min < v.z < z_max]
    return Vector(((min(v.x for v in region) + max(v.x for v in region)) / 2, min(v.y for v in region)))


body_sections, body_top, body_verts = sections(body, 0.8, 1.0)
head_width = max(w for z, w in body_sections if z > body_top - 0.15)
body_neck_z = next(z for z, w in body_sections if w < 1.25 * head_width)
bust_sections, head_top, head_verts = sections(head, 0.0, 0.4)
head_neck_z = min(bust_sections, key=lambda s: s[1])[0]
scale = (body_top - body_neck_z) / (head_top - head_neck_z)
# Face band: 30-70 % of the head height above the neck (eyes, nose, cheeks; not the beard tip, not the crown).
body_c = region_centre(body_verts, body_neck_z + 0.3 * (body_top - body_neck_z), body_neck_z + 0.7 * (body_top - body_neck_z))
head_c = region_centre(head_verts, head_neck_z + 0.3 * (head_top - head_neck_z), head_neck_z + 0.7 * (head_top - head_neck_z))
move = Matrix.Translation(Vector((body_c.x, body_c.y, body_neck_z)))
head.data.transform(move @ Matrix.Scale(scale, 4) @ Matrix.Translation(-Vector((head_c.x, head_c.y, head_neck_z))))
head.data.update()
log("body head %.3f-%.3f (width %.3f) | bust neck %.3f crown %.3f -> scale %.3f" % (body_neck_z, body_top, head_width,
                                                                                    head_neck_z, head_top, scale))

# Cut: the body loses its old head inside a cylinder about the head axis (the scarf around it stays); the new head
# keeps its neck 3 cm further down, ending inside the scarf.
# Head axis: the face front plus half a head's depth back.
axis = Vector((body_c.x, body_c.y + 0.6 * head_width))
cut_z = body_neck_z - 0.008
radius = 1.0 * head_width
beard_y = body_c.y + 0.35 * head_width   # the old beard hangs in front of the scarf below the cut


def cut(obj, keep):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    doomed = [f for f in bm.faces if not keep(sum((v.co for v in f.verts), Vector()) / len(f.verts))]
    bmesh.ops.delete(bm, geom=doomed, context="FACES")
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()


cut(body, lambda p: not ((p.z > cut_z and (Vector((p.x, p.y)) - axis).length < radius)
                         or (p.z > cut_z - 0.09 and p.y < beard_y and abs(p.x - axis.x) < 0.45 * head_width)))
cut(head, lambda p: p.z > cut_z - 0.045)
log("body %d vertices, head %d vertices after the cut at z %.3f" % (len(body.data.vertices), len(head.data.vertices), cut_z))



# Scar (the reference: a healed cut from the forehead across the LEFT eyebrow, and a second one down the left cheek).
# Painted into the head's base colour: points along the path are projected onto the face from the front, their UVs
# found on the hit triangle, and a soft pinkish line is stamped into the texture. The character looks along -Y, so his
# left side is +X.
def head_image():
    for mat in head.data.materials:
        for node in mat.node_tree.nodes if mat and mat.use_nodes else []:
            if node.type == "TEX_IMAGE":
                for link in mat.node_tree.links:
                    if link.from_node == node and link.to_socket.name == "Base Color":
                        return node.image
    return None


def surface_uv(bvh, verts, polys, uvs, point):
    loc, normal, index, _ = bvh.ray_cast(Vector((point.x, point.y - 0.3, point.z)), Vector((0, 1, 0)), 0.6)
    if loc is None:
        return None
    poly = polys[index]
    for i in range(1, len(poly) - 1):
        a, b, c = (verts[poly[0]], verts[poly[i]], verts[poly[i + 1]])
        if geometry.intersect_point_tri(loc, a, b, c) or i == len(poly) - 2:
            ua, ub, uc = (uvs[index][0], uvs[index][i], uvs[index][i + 1])
            uv = geometry.barycentric_transform(loc, a, b, c, ua.to_3d(), ub.to_3d(), uc.to_3d())
            # Texel density of this triangle (UV area per world area), for a stamp of constant real size.
            world = geometry.area_tri(a, b, c)
            uv_area = geometry.area_tri(ua.to_3d(), ub.to_3d(), uc.to_3d())
            density = (uv_area / world) ** 0.5 if world > 1e-12 else 0.0
            return uv.xy, loc, density


def paint_scar():
    image = head_image()
    if image is None:
        log("scar: no head base colour texture")
        return
    verts = [v.co.copy() for v in head.data.vertices]
    polys = [list(p.vertices) for p in head.data.polygons]
    uv_data = head.data.uv_layers.active.data
    uvs = [[uv_data[li].uv.copy() for li in p.loop_indices] for p in head.data.polygons]
    bvh = BVHTree.FromPolygons(verts, polys)
    # Nose tip: frontmost point on the face's centre line (loose strands hanging over the forehead are off-centre).
    face = [v for v in verts if body_neck_z + 0.35 * (body_top - body_neck_z) < v.z < body_neck_z + 0.6 * (body_top - body_neck_z)
            and abs(v.x - axis.x) < 0.012]
    nose = min(face, key=lambda v: v.y)
    log("scar: nose tip at (%.3f %.3f %.3f)" % (nose.x, nose.y, nose.z))
    w, h = image.size
    pixels = np.array(image.pixels[:], dtype=np.float32).reshape(h, w, 4)
    scar_colour = np.array([0.86, 0.52, 0.50])   # healed tissue: lighter and pinker than the tanned skin
    strokes = [  # (path relative to the nose tip, half width in metres)
        ([(0.022, 0.095), (0.030, 0.062), (0.036, 0.040), (0.040, 0.030)], 0.0036),
        ([(0.050, 0.012), (0.054, -0.005), (0.056, -0.025)], 0.003),
    ]
    stamped = 0
    for path, half in strokes:
        for (x0, z0), (x1, z1) in zip(path, path[1:]):
            for t in np.linspace(0.0, 1.0, 40):
                p = nose + Vector((x0 + (x1 - x0) * t, 0.0, z0 + (z1 - z0) * t))
                hit = surface_uv(bvh, verts, polys, uvs, p)
                if hit is None:
                    continue
                cx, cy = hit[0].x * w, hit[0].y * h
                radius = min(max(2.0, half * hit[2] * w), 40.0)
                r = int(radius * 2) + 2
                x_lo, x_hi = max(0, int(cx) - r), min(w, int(cx) + r)
                y_lo, y_hi = max(0, int(cy) - r), min(h, int(cy) + r)
                if x_lo >= x_hi or y_lo >= y_hi:
                    continue
                yy, xx = np.mgrid[y_lo:y_hi, x_lo:x_hi]
                d = np.hypot(xx - cx, yy - cy) / radius
                core = np.clip(1.2 - d, 0, 1)[..., None] * 0.8
                rim = (np.clip(1.6 - d, 0, 1) - np.clip(1.2 - d, 0, 1))[..., None] * 0.2
                block = pixels[y_lo:y_hi, x_lo:x_hi, :3]
                block = block * (1 - core) + scar_colour * core
                block = block * (1 - rim * 0.5)              # slightly darker edge of the healed cut
                pixels[y_lo:y_hi, x_lo:x_hi, :3] = block
                stamped += 1
    image.pixels[:] = pixels.ravel()
    image.update()
    image.pack()
    log("scar painted (%d stamps)" % stamped)


paint_scar()

bpy.ops.object.select_all(action="DESELECT")
body.select_set(True)
head.select_set(True)
bpy.context.view_layer.objects.active = body
bpy.ops.object.join()
merged = bpy.context.view_layer.objects.active
merged.name = "model"
log("materials %s" % [m.name for m in merged.data.materials])
bpy.ops.export_scene.gltf(filepath=OUT, export_format="GLB", use_selection=True)
log("exported " + OUT)
