# Run with Blender 5.x in background mode:
#   blender -b --python Tools/Art/blender_make_akaza_hair.py -- [SK_Akaza.fbx] [out folder]
#
# Strand hair (groom) for Akaza, grown on his rigged head (SourceArt/Hyper3D/Akaza/export/SK_Akaza.fbx, made by
# Tools/Art/blender_merge_head.py + Tools/UE58/blender_rig_hyper3d.py), following the reference sheet:
#   - head hair: curly strands combed back over the scalp into a top knot on the back of the crown,
#   - top knot: strands wound around the bun,
#   - loose curls hanging at the temples, behind the ears and down the nape,
#   - full curly beard and moustache.
# Where hair and beard grow is read from the head's own texture (dark hair colour on the head material); strands start
# on that surface and are kept just above it. Output: Akaza_Hair.abc with two objects (Hair, Beard - two groom groups
# in Unreal, Tools/UE58/db_import_akaza_groom.py) in centimetres, plus a check render.
import math
import os
import random
import sys

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

PROJECT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
EXPORT = os.path.join(PROJECT, "SourceArt", "Hyper3D", "Akaza", "export")
FBX = os.path.abspath(args[0]) if len(args) > 0 and args[0] else os.path.join(EXPORT, "SK_Akaza.fbx")
OUT = os.path.abspath(args[1]) if len(args) > 1 and args[1] else EXPORT
TEXTURE = os.path.join(os.path.dirname(FBX), "T_Akaza_Head_BaseColor.png")
random.seed(23)
rng = np.random.default_rng(23)

COUNTS = {"scalp": 24000, "bun": 3000, "loose": 450, "beard": 12000}


def log(message):
    print("DBHAIR " + message, flush=True)


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


# ---- Head ----------------------------------------------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=FBX)
mesh_obj = next(o for o in bpy.data.objects if o.type == "MESH")
mesh = mesh_obj.data
world = mesh_obj.matrix_world.copy()
verts = [world @ v.co for v in mesh.vertices]
height = max(v.z for v in verts) - min(v.z for v in verts)
unit = 0.01 if height > 50 else 1.0          # the FBX is in centimetres; work in metres
verts = [v * unit for v in verts]
head_slot = next(i for i, m in enumerate(mesh.materials) if m and m.name.startswith("Head"))
uv_layer = mesh.uv_layers.active.data
head_polys = [p for p in mesh.polygons if p.material_index == head_slot]
log("body height %.2f m, head material slot %d with %d polygons" % (height * unit, head_slot, len(head_polys)))

image = bpy.data.images.load(TEXTURE)
tw, th = image.size
pixels = np.array(image.pixels[:], dtype=np.float32).reshape(th, tw, 4)[:, :, :3]
log("head texture %dx%d" % (tw, th))


def colour_at(uv):
    x = min(tw - 1, max(0, int(uv.x % 1.0 * tw)))
    y = min(th - 1, max(0, int(uv.y % 1.0 * th)))
    return pixels[y, x]


head_verts = sorted({i for p in head_polys for i in p.vertices})
head_pts = [verts[i] for i in head_verts]
top = max(p.z for p in head_pts)
centre_x = (min(p.x for p in head_pts) + max(p.x for p in head_pts)) / 2
front = [p for p in head_pts if abs(p.x - centre_x) < 0.012 and top - 0.16 < p.z < top - 0.08]
nose = min(front, key=lambda p: p.y)          # the face looks along -Y
back_y = max(p.y for p in head_pts if p.z > nose.z)
log("crown %.3f, nose tip (%.3f %.3f %.3f), back of the head y %.3f" % (top, nose.x, nose.y, nose.z, back_y))

# Classify head polygons: hair = dark texture. Scalp above the brows, beard below the nose and in front.
scalp, beard = [], []
for poly in head_polys:
    centre = sum((verts[i] for i in poly.vertices), Vector()) / len(poly.vertices)
    uv = sum((uv_layer[li].uv for li in poly.loop_indices), Vector((0.0, 0.0))) / len(poly.loop_indices)
    c = colour_at(uv)
    lum = 0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2]
    normal = (world.to_3x3() @ poly.normal).normalized()
    if lum > 0.16:
        continue
    corners = [verts[i] for i in poly.vertices]
    area = 0.5 * ((corners[1] - corners[0]).cross(corners[2] - corners[0])).length if len(corners) >= 3 else 0.0
    side = abs(centre.x - centre_x) > 0.055
    # Scalp: above the brows; on the sides above the ears (strands must not grow on the cheeks and run over the ears).
    # The nape counts too (it is combed up into the knot).
    if (centre.z > nose.z + (0.06 if side else 0.035)) or (centre.y > back_y - 0.045 and centre.z > nose.z - 0.03):
        scalp.append((corners, normal, area))
    elif nose.z - 0.13 < centre.z < nose.z - 0.005 and centre.y < nose.y + 0.11:
        beard.append((corners, normal, area))
log("scalp %d polygons, beard %d polygons" % (len(scalp), len(beard)))

head_bvh = BVHTree.FromPolygons(verts, [list(p.vertices) for p in head_polys])


def sample_roots(polys, count):
    areas = np.array([a for _, _, a in polys])
    picks = rng.choice(len(polys), size=count, p=areas / areas.sum())
    roots = []
    for k in picks:
        corners, normal, _ = polys[k]
        a, b = rng.random(2)
        if a + b > 1:
            a, b = 1 - a, 1 - b
        roots.append((corners[0] + (corners[1] - corners[0]) * a + (corners[2] - corners[0]) * b, normal))
    return roots


def above_shell(p, offset):
    loc, normal, _, _ = head_bvh.find_nearest(p)
    if loc is None:
        return p
    depth = (p - loc).dot(normal)
    return p if depth >= offset else p + normal * (offset - depth)


def curl(points, amplitude, period, phase):
    """Helical offset around the strand's own direction - curly hair."""
    out = [points[0]]
    travelled = 0.0
    for i in range(1, len(points)):
        d = (points[i] - points[i - 1])
        travelled += d.length
        d = d.normalized() if d.length > 1e-6 else Vector((0, 0, 1))
        side = d.cross(Vector((0, 0, 1)))
        side = side.normalized() if side.length > 1e-4 else Vector((1, 0, 0))
        up = d.cross(side).normalized()
        a = amplitude(i / (len(points) - 1))
        angle = phase + 2 * math.pi * travelled / period
        out.append(points[i] + side * math.cos(angle) * a + up * math.sin(angle) * a)
    return out


# Bun: the backmost hair high on the head.
scalp_centres = [sum(c, Vector()) / len(c) for c, _, _ in scalp]
bun_pts = [p for p in scalp_centres if p.y > back_y - 0.035 and p.z > top - 0.11]
bun = sum(bun_pts, Vector()) / max(1, len(bun_pts)) if bun_pts else Vector((centre_x, back_y, top - 0.05))
bun += Vector((0, 0.02, 0.005))
log("top knot at (%.3f %.3f %.3f) from %d points" % (bun.x, bun.y, bun.z, len(bun_pts)))

strands = {"Hair": [], "Beard": []}

# Head hair: combed back over the scalp into the knot.
for root, normal in sample_roots(scalp, COUNTS["scalp"]):
    target = bun + Vector(rng.normal(0, 0.012, 3))
    steps = 40                                  # dense points: a curl needs ~10 points per turn to stay round
    pts = []
    for k in range(steps + 1):
        t = k / steps
        p = root.lerp(target, t)
        # The comb lifts slightly over the crown, lies on the scalp elsewhere.
        if t < 0.9:
            p = above_shell(p, 0.0012 + 0.0018 * t + 0.0008 * random.random())   # combed flat onto the scalp
        pts.append(p)
    pts = curl(pts, lambda t: 0.0008 + 0.0014 * smoothstep(t * 1.5), 0.028 + 0.01 * random.random(), random.random() * 6.28)
    strands["Hair"].append(pts)

# Top knot: strands wound once or twice around the bun.
for _ in range(COUNTS["bun"]):
    axis = Vector(rng.normal(0, 1, 3)).normalized()
    ref = axis.cross(Vector((0, 0, 1)))
    ref = ref.normalized() if ref.length > 1e-3 else Vector((1, 0, 0))
    ref2 = axis.cross(ref)
    radius = 0.025 + 0.02 * random.random()
    turns = 0.8 + 0.8 * random.random()
    start = random.random() * 6.28
    pts = []
    for k in range(25):
        a = start + turns * 6.28 * k / 24
        pts.append(bun + (ref * math.cos(a) + ref2 * math.sin(a)) * radius)
    strands["Hair"].append(curl(pts, lambda t: 0.003, 0.02, random.random() * 6.28))

# Loose curls: temples, behind the ears, nape - hanging down.
# Only at the temples, behind the ears and at the nape - never in front of the face.
low_roots = [(r, n) for r, n in sample_roots(scalp, COUNTS["loose"] * 8)
             if r.y > nose.y + 0.07 and (r.z < nose.z + 0.09 or (r.y > back_y - 0.03 and r.z < bun.z - 0.04))]
for root, normal in low_roots[:COUNTS["loose"]]:
    length = 0.05 + 0.06 * random.random()
    direction = (normal * 0.35 + Vector((0, 0, -1))).normalized()
    steps = 36
    pts = [above_shell(root + direction * length * k / steps, 0.004) for k in range(steps + 1)]
    strands["Hair"].append(curl(pts, lambda t: 0.0015 + 0.0035 * smoothstep(t * 2), 0.02 + 0.008 * random.random(), random.random() * 6.28))

# Beard: short, dense, curly; longer at the chin.
for root, normal in sample_roots(beard, COUNTS["beard"]):
    chin = smoothstep((nose.z - 0.07 - root.z) / 0.04)
    length = 0.008 + 0.008 * random.random() + 0.01 * chin
    direction = (normal + Vector((0, 0, -0.8))).normalized()
    steps = 10
    pts = [root + direction * length * k / steps for k in range(steps + 1)]
    strands["Beard"].append(curl(pts, lambda t: 0.0012 * t, 0.012, random.random() * 6.28))

# ---- Curves, export -----------------------------------------------------------------------------------------------------
objects = []
for name, items in strands.items():
    curve = bpy.data.curves.new(name, "CURVE")
    curve.dimensions = "3D"
    for pts in items:
        spline = curve.splines.new("POLY")
        spline.points.add(len(pts) - 1)
        flat = []
        for p in pts:
            flat += [p.x, p.y, p.z, 1.0]
        spline.points.foreach_set("co", flat)
    obj = bpy.data.objects.new(name, curve)
    bpy.context.scene.collection.objects.link(obj)
    objects.append(obj)
    log("%s: %d strands, %d points" % (name, len(items), sum(len(p) for p in items)))

bpy.ops.object.select_all(action="DESELECT")
for obj in objects:
    obj.select_set(True)
bpy.context.view_layer.objects.active = objects[0]
abc = os.path.join(OUT, "Akaza_Hair.abc")
bpy.ops.wm.alembic_export(filepath=abc, selected=True, start=1, end=1, global_scale=100.0, export_hair=False,
                          export_particles=False, curves_as_mesh=False)
log("exported " + abc)

# ---- Check render (hair as thin tubes on the head) ----------------------------------------------------------------------
for obj in objects:
    obj.data.bevel_depth = 0.00025 if obj.name == "Hair" else 0.0003
    mat = bpy.data.materials.new(obj.name + "_Check")
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (0.02, 0.014, 0.01, 1)
    bsdf.inputs["Roughness"].default_value = 0.45
    obj.data.materials.append(mat)
for o in bpy.data.objects:
    if o.type == "MESH":
        o.scale = (unit, unit, unit) if unit != 1.0 else o.scale
scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = scene.render.resolution_y = 900
scene.view_settings.view_transform = "AgX"
w = bpy.data.worlds.new("W")
w.use_nodes = True
w.node_tree.nodes["Background"].inputs["Color"].default_value = (0.35, 0.35, 0.36, 1)
scene.world = w
for e, r in ((3.0, (0.9, 0, -0.5)), (1.2, (1.1, 0, 2.6))):
    light = bpy.data.objects.new("S", bpy.data.lights.new("S", "SUN"))
    light.data.energy = e
    light.rotation_euler = r
    scene.collection.objects.link(light)
cam = bpy.data.objects.new("C", bpy.data.cameras.new("C"))
scene.collection.objects.link(cam)
scene.camera = cam
cam.data.type = "ORTHO"
cam.data.ortho_scale = 0.42
for view, loc, rot in (("front", (centre_x, -3, top - 0.13), (math.pi / 2, 0, 0)),
                       ("side", (3, nose.y + 0.08, top - 0.13), (math.pi / 2, 0, math.pi / 2)),
                       ("back", (centre_x, 3, top - 0.13), (math.pi / 2, 0, math.pi))):
    cam.location = loc
    cam.rotation_euler = rot
    scene.render.filepath = os.path.join(OUT, "Akaza_Hair_check_%s.png" % view)
    bpy.ops.render.render(write_still=True)
log("check renders written")
