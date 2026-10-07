# Run with Blender 5.x in background mode:
#   blender -b --python Tools/UE58/blender_metahuman_head_target.py -- <model.glb> <out folder>
#
# Prepares a generated character (Hyper3D Rodin GLB, facing -Y, Z up) as the head target for MetaHuman Creator's
# conform (Tools/UE58/db_conform_metahuman_head.py):
#   head_target.json  head vertices + triangles in Unreal space (cm, X forward, Y right, Z up; the face looks along
#                     +Y like a MetaHuman), the model scaled to 180 cm
#   head_portrait.png front portrait, lit, 1024 x 1024, from the camera stored in the JSON (location, yaw -90,
#                     horizontal FOV) - MetaHuman tracks the face landmarks on it and back-projects them with that camera
import json
import math
import os
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
SOURCE, OUT = args[0], args[1]
HEIGHT = 1.80
HEAD_FRACTION = 0.155  # top part of the body kept as head target (crown to below the chin)
CAMERA_DISTANCE = 0.75
CAMERA_FOV = 30.0
os.makedirs(OUT, exist_ok=True)


def log(message):
    print("DBMHHEAD " + message)


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=SOURCE)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
bpy.ops.object.select_all(action="DESELECT")
for obj in meshes:
    obj.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
if len(meshes) > 1:
    bpy.ops.object.join()
body = bpy.context.view_layer.objects.active
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

points = [v.co for v in body.data.vertices]
low = min(p.z for p in points)
high = max(p.z for p in points)
feet = [p for p in points if p.z < low + 0.15 * (high - low)]
center = Vector((sum(p.x for p in feet) / len(feet), sum(p.y for p in feet) / len(feet), low))
scale = HEIGHT / (high - low)
for vertex in body.data.vertices:
    vertex.co = (vertex.co - center) * scale
body.data.update()
toes = [v.co.y for v in body.data.vertices if v.co.z < 0.06]
if toes and sum(toes) / len(toes) > 0.02:  # facing +Y: turn to -Y
    for vertex in body.data.vertices:
        vertex.co.x, vertex.co.y = -vertex.co.x, -vertex.co.y
    body.data.update()

# Head target: everything above the cut, triangulated.
cut = HEIGHT * (1.0 - HEAD_FRACTION)
head_polys = [p for p in body.data.polygons if all(body.data.vertices[i].co.z > cut for i in p.vertices)]
used = sorted({i for p in head_polys for i in p.vertices})
remap = {old: new for new, old in enumerate(used)}
vertices = []
for old in used:
    co = body.data.vertices[old].co
    vertices.append([co.x * 100.0, -co.y * 100.0, co.z * 100.0])  # Blender m -> Unreal cm (Y mirrored)
triangles = []
for poly in head_polys:
    ids = [remap[i] for i in poly.vertices]
    for k in range(1, len(ids) - 1):
        triangles.extend([ids[0], ids[k + 1], ids[k]])  # winding flips with the mirrored Y
head_center = Vector((sum(v[0] for v in vertices), sum(v[1] for v in vertices), sum(v[2] for v in vertices))) / len(vertices)
face_z = sum(v[2] for v in vertices if v[2] < head_center.z + 6.0 and v[2] > head_center.z - 10.0) / max(
    1, sum(1 for v in vertices if head_center.z - 10.0 < v[2] < head_center.z + 6.0))
log("head target: %d vertices, %d triangles, cut at %.2f m, center %s" % (len(vertices), len(triangles) // 3, cut, tuple(round(c, 1) for c in head_center)))

# Portrait camera (Unreal: at +Y looking along -Y at the face).
camera_ue = [head_center.x, head_center.y + CAMERA_DISTANCE * 100.0, face_z]
camera_data = bpy.data.cameras.new("Portrait")
camera_data.sensor_fit = "HORIZONTAL"
camera_data.angle = math.radians(CAMERA_FOV)
camera = bpy.data.objects.new("Portrait", camera_data)
bpy.context.collection.objects.link(camera)
camera.location = (camera_ue[0] / 100.0, -camera_ue[1] / 100.0, camera_ue[2] / 100.0)
camera.rotation_euler = (math.radians(90.0), 0.0, 0.0)  # looks along +Y in Blender = -Y in Unreal

scene = bpy.context.scene
scene.camera = camera
engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
scene.render.engine = next((e for e in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE") if e in engines), "BLENDER_WORKBENCH")
scene.render.resolution_x = scene.render.resolution_y = 1024
world = bpy.data.worlds.new("Portrait")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.5, 0.5, 0.5, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.2
scene.world = world
key = bpy.data.objects.new("Key", bpy.data.lights.new("Key", "SUN"))
key.data.energy = 3.0
key.rotation_euler = (math.radians(70.0), 0.0, math.radians(15.0))
bpy.context.collection.objects.link(key)
scene.render.filepath = os.path.join(OUT, "head_portrait.png")
bpy.ops.render.render(write_still=True)

with open(os.path.join(OUT, "head_target.json"), "w") as handle:
    json.dump({"vertices": vertices, "triangles": triangles,
               "camera": {"location": camera_ue, "yaw": -90.0, "fov": CAMERA_FOV, "width": 1024, "height": 1024}}, handle)
log("written %s" % OUT)
