# Run with Blender 5.x in background mode:
#   blender -b --python Tools/UE58/blender_rig_hyper3d.py -- <model.glb> <out folder> <Name> [max triangles]
#
# Turns a Hyper3D Rodin character (GLB, A/T pose, one textured mesh) into a game body on the UE5 mannequin skeleton:
#   1. the mannequin armature from SourceArt/Mannequin/SKM_Manny_Simple.fbx (Tools/UE58/db_export_mannequin_fbx.py) is
#      the rig: same bones and hierarchy, so the body imports onto SK_Mannequin and plays every mannequin animation;
#   2. the model is scaled to the mannequin's height, feet on the ground, facing -Y like the mannequin;
#   3. each arm chain is turned and stretched about the shoulder onto the model's arm (hand found as the outermost
#      geometry at hand height), so the A pose of the model and the rig agree;
#   4. loose floating splatter is removed; skin weights by distance to the bone segments (Blender's heat weighting
#      fails on open, self-intersecting AI meshes);
#   5. decimated to the triangle budget (FPS, default 120k), exported as SK_<Name>.fbx plus the textures as PNG, and two check
#      renders (rest pose and a test pose with raised arms and a step) as <Name>_check_*.png.
import math
import os
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(args) < 3:
    raise SystemExit("usage: blender -b --python blender_rig_hyper3d.py -- <model.glb> <out folder> <Name> [max triangles]")
SOURCE, OUT, NAME = args[0], args[1], args[2]
MAX_TRIS = int(args[3]) if len(args) > 3 else 120000
PROJECT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MANNY = os.path.join(PROJECT, "SourceArt", "Mannequin", "SKM_Manny_Simple.fbx")
os.makedirs(OUT, exist_ok=True)


def log(message):
    print("DBRIG " + message)


def select_only(*objects):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


bpy.ops.wm.read_factory_settings(use_empty=True)

# 1. Rig: the mannequin armature in meters, scale 1, without its mesh.
bpy.ops.import_scene.fbx(filepath=MANNY)
armature = next(o for o in bpy.data.objects if o.type == "ARMATURE")
select_only(armature)
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for obj in [o for o in bpy.data.objects if o is not armature]:
    bpy.data.objects.remove(obj, do_unlink=True)
armature.name = "root"  # exported as the root bone, as in SK_Mannequin
bones = armature.data.bones
rig_height = max((armature.matrix_world @ b.tail_local).z for b in bones if b.name == "head")
rig_height = max(rig_height, max((armature.matrix_world @ b.head_local).z for b in bones)) + 0.12
log("mannequin rig %d bones, height %.2f m" % (len(bones), rig_height))

# 2. Model: one mesh, transforms applied, scaled to the mannequin height, feet at z=0, centered.
before = set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=SOURCE)
meshes = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
for obj in [o for o in bpy.data.objects if o not in before and o.type != "MESH"]:
    for child in obj.children:
        child.matrix_parent_inverse = obj.matrix_world.inverted()
select_only(*meshes)
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
if len(meshes) > 1:
    bpy.ops.object.join()
body = bpy.context.view_layer.objects.active
for obj in [o for o in bpy.data.objects if o not in (armature, body)]:
    bpy.data.objects.remove(obj, do_unlink=True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
body.name = "SK_" + NAME
points = [v.co for v in body.data.vertices]
low = min(p.z for p in points)
high = max(p.z for p in points)
feet = [p for p in points if p.z < low + 0.15 * (high - low)]
center = Vector((sum(p.x for p in feet) / len(feet), sum(p.y for p in feet) / len(feet), low))
scale = 1.80 / (high - low)
body.data.transform(Matrix.Scale(scale, 4) @ Matrix.Translation(-center))
body.data.update()
log("model %d vertices, %d polygons, source height %.3f -> scale %.3f" % (len(body.data.vertices), len(body.data.polygons), high - low, scale))

# Facing: the mannequin looks along -Y. The chest/face side of a character carries more geometry in front of the spine
# line at chest height than behind it (cape and hair hang behind) - compare the mean y of the front and back halves.
chest = [v.co for v in body.data.vertices if 1.15 < v.co.z < 1.55 and abs(v.co.x) < 0.18]
mean_y = sum(p.y for p in chest) / max(len(chest), 1)
toes = [v.co for v in body.data.vertices if v.co.z < 0.06]
toe_y = sum(p.y for p in toes) / max(len(toes), 1)
log("facing check: chest mean y %.3f, toe mean y %.3f (mannequin: toes at -y)" % (mean_y, toe_y))
if toe_y > 0.02:
    body.data.transform(Matrix.Rotation(math.pi, 4, "Z"))
    body.data.update()
    log("model turned 180 degrees to face -Y")

# 3. Arms: turn and stretch each chain about its shoulder onto the model's arm.
select_only(armature)
bpy.ops.object.mode_set(mode="EDIT")
edit = armature.data.edit_bones


def descendants(bone):
    result = []
    for child in bone.children:
        result.append(child)
        result.extend(descendants(child))
    return result


verts = [v.co.copy() for v in body.data.vertices]
for side, sign in (("l", 1.0), ("r", -1.0)):
    upper = edit["upperarm_" + side]
    hand = edit["hand_" + side]
    shoulder = upper.head.copy()
    # A/T pose: the hand is the outermost geometry beside and below the shoulder.
    candidates = [p for p in verts if sign * p.x > sign * shoulder.x + 0.25 and 0.75 < p.z < shoulder.z]
    margin = 0.04
    if len(candidates) < 50:
        # Hanging arms: the hands are the outermost geometry at hip height (above the widening skirt/hakama).
        candidates = [p for p in verts if sign * p.x > 0.15 and 0.80 < p.z < 1.0]
        margin = 0.025
        log("%s arm: hanging arm, hand searched at hip height (%d points)" % (side, len(candidates)))
    if len(candidates) < 50:
        log("%s arm: no hand found, rig arm kept" % side)
        continue
    reach = max(sign * p.x for p in candidates)
    tip_points = [p for p in candidates if sign * p.x > reach - margin]
    tip = sum(tip_points, Vector()) / len(tip_points)
    current_tip = hand.head + (hand.head - edit["lowerarm_" + side].head).normalized() * 0.17
    old = current_tip - shoulder
    new = tip - shoulder
    rotation = old.rotation_difference(new).to_matrix().to_4x4()
    stretch = new.length / old.length
    transform = Matrix.Translation(shoulder) @ rotation @ Matrix.Scale(stretch, 4) @ Matrix.Translation(-shoulder)
    for bone in [upper] + descendants(upper):
        roll_axis = bone.z_axis.copy()
        bone.head = transform @ bone.head
        bone.tail = transform @ bone.tail
        bone.align_roll(rotation.to_3x3() @ roll_axis)
    log("%s arm: fingertips at (%.2f %.2f %.2f), turned %.1f deg, stretch %.2f" % (
        side, tip.x, tip.y, tip.z, math.degrees(old.angle(new)), stretch))
bpy.ops.object.mode_set(mode="OBJECT")

# Helper bones of the mannequin (IK targets, interaction, center of mass) must not take skin weights.
for bone in armature.data.bones:
    bone.use_deform = not (bone.name.startswith("ik_") or bone.name in ("root", "interaction", "center_of_mass"))

# 5a. Triangle budget before skinning (weights then follow the final topology).
triangles = sum(len(p.vertices) - 2 for p in body.data.polygons)
if triangles > MAX_TRIS:
    select_only(body)
    decimate = body.modifiers.new("Budget", "DECIMATE")
    decimate.ratio = MAX_TRIS / triangles
    decimate.use_collapse_triangulate = False
    bpy.ops.object.modifier_apply(modifier=decimate.name)
    log("decimated %d -> %d triangles" % (triangles, sum(len(p.vertices) - 2 for p in body.data.polygons)))
else:
    log("%d triangles, within the budget" % triangles)

# 4. Weights by distance to the bone segments. Blender's heat weighting needs closed, non-intersecting meshes and
# fails on AI output; distance weights (1/d^6, the four nearest bones, normalized) are robust and predictable for an
# A-pose body whose limbs stand apart. Twist/corrective/IK helpers stay unweighted (UE drives them separately).
SKIN = [b.name for b in armature.data.bones if b.use_deform and "twist" not in b.name and "corrective" not in b.name
        and not b.name.startswith(("ik_", "weapon"))]
segments = [(name, armature.matrix_world @ armature.data.bones[name].head_local,
             armature.matrix_world @ armature.data.bones[name].tail_local) for name in SKIN]


def segment_distance(point, head, tail):
    axis = tail - head
    length = axis.length_squared
    t = 0.0 if length == 0.0 else max(0.0, min(1.0, (point - head).dot(axis) / length))
    return (point - (head + axis * t)).length


# Loose splatter (blood drops baked into the concept art) floats beside the body: islands whose every vertex is far
# from all bones are removed; the game adds blood as particles.
mesh = bmesh.new()
mesh.from_mesh(body.data)
mesh.verts.ensure_lookup_table()
seen = set()
removed = 0
for start in mesh.verts:
    if start.index in seen:
        continue
    island, stack = [], [start]
    seen.add(start.index)
    while stack:
        vert = stack.pop()
        island.append(vert)
        for edge in vert.link_edges:
            other = edge.other_vert(vert)
            if other.index not in seen:
                seen.add(other.index)
                stack.append(other)
    nearest = min(min(segment_distance(v.co, h, t) for _, h, t in segments) for v in island[:: max(1, len(island) // 40)])
    # Only small specks far out: Rodin's detailed models are made of many separate cloth and armor pieces.
    if nearest > 0.35 and len(island) < 0.002 * len(mesh.verts):
        bmesh.ops.delete(mesh, geom=island, context="VERTS")
        removed += 1
mesh.to_mesh(body.data)
mesh.free()
body.data.update()
log("removed %d loose floating islands, %d vertices remain" % (removed, len(body.data.vertices)))

select_only(body)
groups = {name: body.vertex_groups.new(name=name) for name in SKIN}
LEGS = {"thigh_l", "thigh_r", "calf_l", "calf_r"}
pelvis_z = (armature.matrix_world @ armature.data.bones["pelvis"].head_local).z
# Hanging arms beside a skirt/hakama: cloth below the hip inward of the hands never belongs to the arm.
ARM_PREFIXES = ("upperarm", "lowerarm", "hand", "thumb", "index", "middle", "ring", "pinky")
hand_x = {side: abs((armature.matrix_world @ armature.data.bones["hand_" + side].head_local).x) for side in ("l", "r")}


def arm_side(name):
    return name[-1] if name.startswith(ARM_PREFIXES) and name[-2:] in ("_l", "_r") else None


for vertex in body.data.vertices:
    side = "l" if vertex.co.x > 0.0 else "r"
    below_hip = vertex.co.z < pelvis_z + 0.1 and abs(vertex.co.x) < hand_x[side] - 0.05
    usable = [(name, h, t) for name, h, t in segments if not (below_hip and arm_side(name))]
    scored = sorted((max(segment_distance(vertex.co, h, t), 0.01), name) for name, h, t in usable)[:4]
    weights = {name: 1.0 / d ** 6 for d, name in scored}
    total = sum(weights.values())
    weights = {name: w / total for name, w in weights.items()}
    # Robes and skirts: cloth hanging well away from the legs follows the pelvis half-way, so a raised leg swings the
    # hem instead of stretching it like rubber.
    leg_weight = sum(w for name, w in weights.items() if name in LEGS)
    if vertex.co.z < pelvis_z and leg_weight > 0.5:
        gap = min(segment_distance(vertex.co, h, t) for name, h, t in segments if name in LEGS)
        share = max(0.0, min(0.5, (gap - 0.07) / 0.15))
        if share > 0.0:
            weights = {name: w * (1.0 - share) for name, w in weights.items()}
            weights["pelvis"] = weights.get("pelvis", 0.0) + share
    for name, weight in weights.items():
        if weight > 0.02:
            groups[name].add([vertex.index], weight, "REPLACE")
bpy.ops.object.vertex_group_normalize_all(lock_active=False)
unweighted = sum(1 for v in body.data.vertices if not v.groups)
log("distance weights on %d bones, %d of %d vertices without weight" % (len(SKIN), unweighted, len(body.data.vertices)))

body.parent = armature
body.matrix_parent_inverse = Matrix.Identity(4)
modifier = body.modifiers.new("Armature", "ARMATURE")
modifier.object = armature

# 5b. Textures as PNG next to the FBX.
for image in bpy.data.images:
    if image.size[0] == 0:
        continue
    usage = "Texture"
    for material in body.data.materials:
        for node in material.node_tree.nodes if material and material.use_nodes else []:
            if node.type == "TEX_IMAGE" and node.image == image:
                for link in material.node_tree.links:
                    if link.from_node == node:
                        socket = link.to_socket.name
                        target = link.to_node
                        if target.type == "NORMAL_MAP" or "Normal" in socket:
                            usage = "Normal"
                        elif socket == "Base Color":
                            usage = "BaseColor"
                        elif target.type in ("SEPARATE_COLOR", "SEPRGB", "SEPARATE_RGB") or socket in ("Roughness", "Metallic"):
                            usage = "ORM"
    path = os.path.join(OUT, "T_%s_%s.png" % (NAME, usage))
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()
    log("texture %s %dx%d -> %s" % (image.name, image.size[0], image.size[1], os.path.basename(path)))

select_only(armature, body)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "SK_%s.fbx" % NAME), use_selection=True, apply_unit_scale=True,
                         apply_scale_options="FBX_SCALE_UNITS", add_leaf_bones=False, bake_anim=False,
                         mesh_smooth_type="FACE", path_mode="STRIP", embed_textures=False, use_armature_deform_only=False,
                         primary_bone_axis="Y", secondary_bone_axis="X", armature_nodetype="NULL")
log("exported %s" % os.path.join(OUT, "SK_%s.fbx" % NAME))

# Check renders: rest pose and a test pose (arms raised, a step), front view.
scene = bpy.context.scene
# EEVEE with the real PBR material and a plain studio light (Workbench shows the wrong texture slot and fake gloss).
engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
scene.render.engine = next((e for e in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE") if e in engines), "BLENDER_WORKBENCH")
world = bpy.data.worlds.new("Check")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.35, 0.35, 0.38, 1.0)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.0
scene.world = world
for name, rotation, energy in (("Key", (math.radians(50), 0.0, math.radians(-30)), 4.0),
                               ("Rim", (math.radians(60), 0.0, math.radians(150)), 2.5)):
    light = bpy.data.objects.new(name, bpy.data.lights.new(name, "SUN"))
    light.data.energy = energy
    light.rotation_euler = rotation
    bpy.context.collection.objects.link(light)
scene.view_settings.view_transform = "AgX" if "AgX" in [v.identifier for v in scene.view_settings.bl_rna.properties["view_transform"].enum_items] else "Filmic"
scene.render.resolution_x, scene.render.resolution_y = 900, 1200
camera_data = bpy.data.cameras.new("Check")
camera_data.type = "ORTHO"
camera_data.ortho_scale = 2.1
camera = bpy.data.objects.new("Check", camera_data)
bpy.context.collection.objects.link(camera)
scene.camera = camera
for label, location, rotation in (("front", (0.0, -6.0, 0.95), (math.radians(90), 0.0, 0.0)),
                                  ("side", (6.0, 0.0, 0.95), (math.radians(90), 0.0, math.radians(90)))):
    camera.location = location
    camera.rotation_euler = rotation
    for pose in ("rest", "test"):
        for bone in armature.pose.bones:
            bone.rotation_mode = "XYZ"
            bone.rotation_euler = (0.0, 0.0, 0.0)
        if pose == "test":
            for name, angles in (("upperarm_l", (0, 0, -50)), ("upperarm_r", (0, 0, -50)), ("lowerarm_l", (0, 0, -40)),
                                 ("lowerarm_r", (0, 0, -40)), ("thigh_l", (0, 0, 35)), ("calf_l", (0, 0, -45)),
                                 ("spine_03", (0, 0, 12)), ("head", (0, 0, 15))):
                armature.pose.bones[name].rotation_euler = tuple(math.radians(a) for a in angles)
        bpy.context.view_layer.update()
        scene.render.filepath = os.path.join(OUT, "%s_check_%s_%s.png" % (NAME, label, pose))
        bpy.ops.render.render(write_still=True)
log("check renders written")
