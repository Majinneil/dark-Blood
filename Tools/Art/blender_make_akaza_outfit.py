# Run with Blender 5.x in background mode:
#   blender -b --python Tools/Art/blender_make_akaza_outfit.py -- [body.fbx] [out folder]
#
# Builds Akaza Kurosaki's outfit (docs/AKAZA_OUTFIT.md) around a skinned body on the UE5 skeleton - the MetaHuman body
# exported from Unreal, or the mannequin (SourceArt/Mannequin/SKM_Manny_Simple.fbx) as a stand-in while developing:
#   kimono (cream, patterned) with wide hanging sleeves and black hem bands; crossed black collar; wrap robe below the
#   belt (black, patterned hem); wide black trousers gathered at the shins with rope bindings; black leather boots;
#   obi with leather belts, buckle, hanging strap with a ring and a cream tassel; thick black hood scarf around the neck
#   with a stole over the left shoulder; diagonal leather strap; forearm wraps.
# Everything is measured from the body (ray casts from the bone axes), so the same script fits another body. Cloth parts
# are dropped onto the body with Blender's cloth solver (folds, sleeves hang). Weights: transferred from the body where
# the cloth lies on it, by distance to the bone segments where it hangs free (sleeves, robe, straps). Output:
#   SK_Akaza_Outfit.fbx (mesh + the body's armature, one material slot per fabric) and check renders front/side/back.
import math
import os
import random
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

PROJECT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
BODY_FBX = args[0] if len(args) > 0 and args[0] else os.path.join(PROJECT, "SourceArt", "Mannequin", "SKM_Manny_Simple.fbx")
OUT = args[1] if len(args) > 1 and args[1] else os.path.join(PROJECT, "SourceArt", "Generated", "AkazaOutfit")
STAGE = args[2] if len(args) > 2 else "all"     # kimono_raw | kimono | lower | all: stop and render after that stage
FABRICS = os.path.join(PROJECT, "SourceArt", "Generated", "AkazaOutfit")
os.makedirs(OUT, exist_ok=True)
random.seed(7)
TAU = 2.0 * math.pi


def log(message):
    print("DBOUTFIT " + message, flush=True)


def select_only(*objects):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def lerp(a, b, t):
    return a + (b - a) * t


# ---- 1. Body ------------------------------------------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
bpy.ops.import_scene.fbx(filepath=BODY_FBX)
armature = next(o for o in bpy.data.objects if o.type == "ARMATURE")
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
select_only(armature, *meshes)
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for obj in [o for o in bpy.data.objects if o.type not in ("ARMATURE", "MESH")]:
    bpy.data.objects.remove(obj, do_unlink=True)
if len(meshes) > 1:
    select_only(*meshes)
    bpy.ops.object.join()
body = [o for o in bpy.data.objects if o.type == "MESH"][0]
body.name = "Body"
armature.name = "root"
for mod in list(body.modifiers):
    body.modifiers.remove(mod)
log("body %d vertices, %d groups, height %.3f" % (len(body.data.vertices), len(body.vertex_groups), body.dimensions.z))


def bone(name, tail=False):
    b = armature.data.bones[name]
    return armature.matrix_world @ (b.tail_local if tail else b.head_local)


PELVIS = bone("pelvis")
NECK = bone("neck_01")
HEAD = bone("head")
SHOULDER = {s: bone("upperarm_" + s) for s in "lr"}
ELBOW = {s: bone("lowerarm_" + s) for s in "lr"}
WRIST = {s: bone("hand_" + s) for s in "lr"}
HIP = {s: bone("thigh_" + s) for s in "lr"}
KNEE = {s: bone("calf_" + s) for s in "lr"}
ANKLE = {s: bone("foot_" + s) for s in "lr"}
BALL = {s: bone("ball_" + s) for s in "lr"}
SPINE = sorted([bone(n) for n in ("pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "neck_01", "neck_02")],
               key=lambda p: p.z)

depsgraph = bpy.context.evaluated_depsgraph_get()
BODY_BVH = BVHTree.FromObject(body, depsgraph)
# The body below the head: the head must not block rays that look for the top of the shoulders.
_verts = [v.co.copy() for v in body.data.vertices]
_low_polys = [list(p.vertices) for p in body.data.polygons if all(_verts[i].z < NECK.z + 0.035 for i in p.vertices)]
TRUNK_BVH = BVHTree.FromPolygons(_verts, _low_polys)


def axis_y(z):
    """The spine line: y of the body's center at height z."""
    if z <= SPINE[0].z:
        return SPINE[0].y
    for a, b in zip(SPINE, SPINE[1:]):
        if z <= b.z:
            return lerp(a.y, b.y, (z - a.z) / max(b.z - a.z, 1e-6))
    return SPINE[-1].y


def first_hit(bvh, origin, direction, limit=0.6):
    hit = bvh.ray_cast(origin, direction.normalized(), limit)
    return hit[3] if hit[0] is not None else None


def last_hit(bvh, origin, direction, limit=0.6):
    """Outermost surface along the ray: cast inward from outside."""
    direction = direction.normalized()
    hit = bvh.ray_cast(origin + direction * limit, -direction, limit)
    return limit - hit[3] if hit[0] is not None else None


def circular_smooth(values, passes=2):
    n = len(values)
    for _ in range(passes):
        values = [(values[i - 1] + 2.0 * values[i] + values[(i + 1) % n]) / 4.0 for i in range(n)]
    return values


def body_ring(z, n, limit=0.28, center=None):
    """Distance from the body axis to the skin for n directions at height z (theta 0 = +x, quarter turn = +y, back)."""
    c = center if center is not None else Vector((0.0, axis_y(z), z))
    radii = []
    for i in range(n):
        a = TAU * i / n
        d = first_hit(BODY_BVH, c, Vector((math.cos(a), math.sin(a), 0.0)), limit)
        radii.append(d if d is not None else (radii[-1] if radii else 0.12))
    return circular_smooth([min(max(r, 0.03), limit) for r in radii], 1)


def limb_ring(start, end, t, n, limit=0.2):
    """Skin distances around a limb axis (start->end) at fraction t, plus the frame used."""
    axis = (end - start).normalized()
    center = start.lerp(end, t)
    ref = Vector((0.0, -1.0, 0.0)) if abs(axis.y) < 0.8 else Vector((0.0, 0.0, 1.0))
    u = (ref - axis * ref.dot(axis)).normalized()
    w = axis.cross(u)
    radii = []
    for i in range(n):
        a = TAU * i / n
        d = first_hit(BODY_BVH, center, u * math.cos(a) + w * math.sin(a), limit)
        radii.append(d if d is not None else (radii[-1] if radii else 0.06))
    return center, u, w, circular_smooth([min(max(r, 0.02), limit) for r in radii], 1)


# ---- Mesh helpers ------------------------------------------------------------------------------------------------------
class Builder:
    """Collects quads with per-corner UVs and material slots, plus named vertex sets, into one bmesh."""

    def __init__(self):
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.sets = {}

    def vert(self, co, *tags):
        v = self.bm.verts.new(co)
        for tag in tags:
            self.sets.setdefault(tag, []).append(v)
        return v

    def quad(self, verts, uvs, material):
        try:
            face = self.bm.faces.new(verts)
        except ValueError:
            return None
        face.material_index = material
        for loop, uv in zip(face.loops, uvs):
            loop[self.uv].uv = uv
        return face

    def grid(self, rows, uvs, material, closed=True, skip=None):
        """rows[j][i] verts, uvs[j][i] (closed grids carry one extra uv column for the seam)."""
        n = len(rows[0])
        cols = n if closed else n - 1
        for j in range(len(rows) - 1):
            for i in range(cols):
                if skip and skip(j, i):
                    continue
                k = (i + 1) % n
                m = material(j, i) if callable(material) else material
                self.quad((rows[j][i], rows[j][k], rows[j + 1][k], rows[j + 1][i]),
                          (uvs[j][i], uvs[j][i + 1], uvs[j + 1][i + 1], uvs[j + 1][i]), m)

    def finish(self, name, materials, recalc=True):
        if recalc:
            bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        self.bm.verts.index_update()
        mesh = bpy.data.meshes.new(name)
        for mat in materials:  # slots first: writing the mesh clamps material indices to the existing slots
            mesh.materials.append(mat)
        self.bm.to_mesh(mesh)
        obj = bpy.data.objects.new(name, mesh)
        scene.collection.objects.link(obj)
        for tag, verts in self.sets.items():
            group = obj.vertex_groups.new(name=tag)
            group.add([v.index for v in verts if v.is_valid], 1.0, "REPLACE")
        self.bm.free()
        for poly in mesh.polygons:
            poly.use_smooth = True
        return obj


def tube(builder, rings, material, u_tile, v_tile, v_start=0.0, closed_ring=True, tags=()):
    """Loft through rings (lists of points); UVs by arc length. Returns the vertex rows."""
    rows = [[builder.vert(p, *tags) for p in ring] for ring in rings]
    uvs = []
    v = v_start
    for j, ring in enumerate(rings):
        if j > 0:
            v += sum((a - b).length for a, b in zip(ring, rings[j - 1])) / len(ring) / v_tile
        u = 0.0
        row = [(0.0, v)]
        pts = ring + [ring[0]] if closed_ring else ring
        for a, b in zip(pts, pts[1:]):
            u += (b - a).length / u_tile
            row.append((u, v))
        uvs.append(row)
    builder.grid(rows, uvs, material, closed=closed_ring)
    return rows


# ---- Materials ---------------------------------------------------------------------------------------------------------
def fabric_material(name, prefix, tint=None, normal_strength=0.4):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes["Principled BSDF"]
    uv = nodes.new("ShaderNodeTexCoord")

    def tex(suffix, color):
        node = nodes.new("ShaderNodeTexImage")
        node.image = bpy.data.images.load(os.path.join(FABRICS, "T_%s_%s.png" % (prefix, suffix)), check_existing=True)
        node.image.colorspace_settings.name = "sRGB" if color else "Non-Color"
        links.new(uv.outputs["UV"], node.inputs["Vector"])
        return node

    base = tex("BC", True)
    if tint:
        mix = nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 1.0
        mix.inputs["B"].default_value = (*tint, 1.0)
        links.new(base.outputs["Color"], mix.inputs["A"])
        links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    else:
        links.new(base.outputs["Color"], bsdf.inputs["Base Color"])
    rough = nodes.new("ShaderNodeMath")
    rough.operation = "MAXIMUM"
    rough.inputs[1].default_value = 0.72 if "Fabric" in prefix else 0.45   # cloth is matte, leather keeps a sheen
    links.new(tex("R", False).outputs["Color"], rough.inputs[0])
    links.new(rough.outputs["Value"], bsdf.inputs["Roughness"])
    normal = nodes.new("ShaderNodeNormalMap")
    normal.inputs["Strength"].default_value = normal_strength
    links.new(tex("N", False).outputs["Color"], normal.inputs["Color"])
    links.new(normal.outputs["Normal"], bsdf.inputs["Normal"])
    if "Sheen Weight" in bsdf.inputs:
        bsdf.inputs["Sheen Weight"].default_value = 0.25 if "Fabric" in prefix else 0.0
    return mat


def plain_material(name, color, roughness, metallic=0.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    return mat


MAT = {
    "Ivory": fabric_material("MI_Akaza_KimonoIvory", "Fabric_IvoryPattern"),
    "Motif": fabric_material("MI_Akaza_RobeBlack", "Fabric_BlackMotif"),
    "Border": fabric_material("MI_Akaza_HemBorder", "Fabric_BlackBorder"),
    "Plain": fabric_material("MI_Akaza_BlackCotton", "Fabric_BlackPlain"),
    "Leather": fabric_material("MI_Akaza_Leather", "Leather_Brown"),
    "Boot": fabric_material("MI_Akaza_BootLeather", "Leather_Brown", tint=(0.32, 0.30, 0.30)),
    "Rope": fabric_material("MI_Akaza_Rope", "Rope_Hemp"),
    "Tassel": plain_material("MI_Akaza_Tassel", (0.62, 0.56, 0.45), 0.85),
    "Metal": plain_material("MI_Akaza_Brass", (0.50, 0.38, 0.20), 0.42, 1.0),
}
SLOTS = list(MAT.keys())


def slot(name):
    return SLOTS.index(name)


ALL_MATS = [MAT[k] for k in SLOTS]


# ---- Cloth simulation --------------------------------------------------------------------------------------------------
def make_collider(obj, thickness=0.006, friction=5.0):
    if not any(m.type == "COLLISION" for m in obj.modifiers):
        obj.modifiers.new("Collision", "COLLISION")
    obj.collision.thickness_outer = thickness
    obj.collision.cloth_friction = friction
    obj.collision.damping = 0.6


def drop_cloth(obj, frames, pin=None, mass=0.3, bending=1.0, tension=15.0, shrink=0.0, self_collision=False):
    """Simulate obj falling onto the colliders for `frames` frames and keep the last frame."""
    push_out(obj, 0.01)
    mod = obj.modifiers.new("Cloth", "CLOTH")
    s = mod.settings
    s.quality = 8
    s.mass = mass
    s.tension_stiffness = tension
    s.compression_stiffness = tension
    s.shear_stiffness = 5.0
    s.bending_stiffness = bending
    s.air_damping = 3.0
    s.shrink_min = shrink
    if pin:
        s.vertex_group_mass = pin
        s.pin_stiffness = 1.0
    c = mod.collision_settings
    c.distance_min = 0.005
    c.use_self_collision = self_collision
    c.self_distance_min = 0.004
    mod.point_cache.frame_start = 1
    mod.point_cache.frame_end = frames
    scene.frame_start = 1
    scene.frame_end = frames
    rest = [v.co.copy() for v in obj.data.vertices]
    for frame in range(1, frames + 1):
        scene.frame_set(frame)
    select_only(obj)
    bpy.ops.object.modifier_apply(modifier=mod.name)
    scene.frame_set(1)
    # A solver blow-up (degenerate input, deep intersection) throws vertices across the scene: keep the input then.
    worst = max((v.co - r).length for v, r in zip(obj.data.vertices, rest))
    if worst > 0.35:
        for v, r in zip(obj.data.vertices, rest):
            v.co = r
        obj.data.update()
        log("WARNING %s: cloth solver diverged (a vertex moved %.2f m) - kept the unsimulated shape" % (obj.name, worst))
    else:
        log("%s: cloth settled over %d frames (largest move %.3f m)" % (obj.name, frames, worst))


def evaluated_bvh(*objects, skip_groups=("Sleeve_l", "Sleeve_r")):
    """Surface of the objects for measuring and projecting - without the sleeves, which hang beside the belt and the
    chest and would catch every ray aimed at the torso."""
    verts, polys = [], []
    dg = bpy.context.evaluated_depsgraph_get()
    for obj in objects:
        skip = {obj.vertex_groups[g].index for g in skip_groups if g in obj.vertex_groups}
        hidden = {v.index for v in obj.data.vertices if any(e.group in skip for e in v.groups)} if skip else set()
        mesh = obj.evaluated_get(dg).to_mesh()
        base = len(verts)
        verts.extend(obj.matrix_world @ v.co for v in mesh.vertices)
        polys.extend([base + i for i in p.vertices] for p in mesh.polygons
                     if not hidden or not any(i in hidden for i in p.vertices))
        obj.evaluated_get(dg).to_mesh_clear()
    return BVHTree.FromPolygons(verts, polys)


def vrows_all(rows):
    return [v for row in rows for v in row]


def push_out(obj, gap, passes=2):
    """Moves every vertex that lies inside the body or closer than gap to it out along the body's normal."""
    for _ in range(passes):
        moved = 0
        for v in obj.data.vertices:
            loc, normal, _, dist = BODY_BVH.find_nearest(v.co)
            if loc is None:
                continue
            depth = (v.co - loc).dot(normal)
            if depth < gap:
                v.co = loc + normal * gap + (v.co - loc - normal * depth)
                moved += 1
        obj.data.update()
    log("%s: %d vertices pushed off the body" % (obj.name, moved))


# ---- 2. Kimono: body, armholes, sleeves --------------------------------------------------------------------------------
N = 72                                   # directions around the body (5 degrees)
BELT_LOW, BELT_HIGH = PELVIS.z - 0.01, PELVIS.z + 0.115
KIMONO_HEM = PELVIS.z - 0.08
TOP = SHOULDER["l"].z + 0.02             # ring through the shoulder joints: the armholes hang below it
CHEST = SHOULDER["l"].z - 0.14
ARMHOLE = 0.20                           # armhole depth below TOP
SIDE_HALF = 4                            # armhole half-width in directions (4 x 5 degrees)
SLEEVE_LENGTH = 0.40
SLEEVE_RINGS = 18
YOKE_ROWS = 6
TILE = 0.42                              # fabric tile size in meters
IVORY_TILE = 0.85                        # the kimono print is large (motifs of a hand's width)


def kimono_radius(z, ring_body, chest_body):
    if z <= BELT_HIGH:
        return [r + 0.013 for r in ring_body]
    blouse = smoothstep((z - BELT_HIGH) / max(CHEST - BELT_HIGH, 0.05))
    ease = 0.03 if z < TOP - 0.06 else 0.024
    # Below the chest the cloth falls straight (it does not follow the waist) and blouses over the belt.
    return [max(r + ease, lerp(r + 0.013, c + ease * 0.8, blouse)) for r, c in zip(ring_body, chest_body)]


def build_kimono():
    b = Builder()
    rows_z = []
    z = KIMONO_HEM
    while z < TOP - 0.005:
        rows_z.append(z)
        z += 0.022
    rows_z.append(TOP)
    chest_body = body_ring(CHEST, N)
    rings = []
    for z in rows_z:
        radii = kimono_radius(z, body_ring(z, N), chest_body)
        c = Vector((0.0, axis_y(z), z))
        rings.append([c + Vector((math.cos(TAU * i / N), math.sin(TAU * i / N), 0.0)) * radii[i] for i in range(N)])
    # Yoke: from the shoulder ring over the shoulders to the collar, lifted onto the top of the shoulders.
    neck_c = Vector((0.0, NECK.y, NECK.z))
    neck = []
    for i in range(N):
        a = TAU * i / N
        zc = NECK.z - 0.035 + 0.035 * math.sin(a)   # low in front, high at the nape
        d = first_hit(BODY_BVH, Vector((0.0, axis_y(zc), zc)), Vector((math.cos(a), math.sin(a), 0.0)), 0.2) or 0.07
        neck.append(Vector((0.0, axis_y(zc), zc)) + Vector((math.cos(a), math.sin(a), 0.0)) * (d + 0.022))
    top = len(rows_z) - 1
    for k in range(1, YOKE_ROWS + 1):
        t = k / YOKE_ROWS
        ring = []
        for i in range(N):
            p = rings[top][i].lerp(neck[i], t) if k < YOKE_ROWS else neck[i].copy()
            if k < YOKE_ROWS:
                hit = TRUNK_BVH.ray_cast(Vector((p.x, p.y, p.z + 0.3)), Vector((0, 0, -1)), 0.6)
                if hit[0] is not None:
                    p.z = max(p.z, hit[0].z + 0.024)
            ring.append(p)
        rings.append(ring)
    top_index = len(rows_z) - 1
    hole_rows = [j for j, z in enumerate(rows_z) if z >= TOP - ARMHOLE - 1e-4 and j < top_index]
    j0 = hole_rows[0]
    sides = {"l": 0, "r": N // 2}

    def in_hole(j, i):
        if not (j0 <= j < top_index):
            return False
        return any(((i - c + N // 2) % N) - N // 2 in range(-SIDE_HALF, SIDE_HALF) for c in sides.values())

    # Torso rows: u by angle at a mean radius, v by height.
    vrows = [[b.vert(p, "Kimono", "Torso") for p in ring] for ring in rings]
    for v in vrows[0]:
        b.sets.setdefault("Pin", []).append(v)
    for j, z in enumerate(rows_z):
        if z <= BELT_HIGH - 0.01:
            b.sets.setdefault("Pin", []).extend(vrows[j])
    b.sets.setdefault("Pin", []).extend(vrows[-1])
    uvs = []
    vacc = 0.0
    for j, ring in enumerate(rings):
        if j > 0:
            vacc += sum((a - c).length for a, c in zip(ring, rings[j - 1])) / N
        uvs.append([(i / N * TAU * 0.16 / IVORY_TILE, vacc / IVORY_TILE) for i in range(N + 1)])  # ~1 m around
    b.grid(vrows, uvs, slot("Ivory"), closed=True, skip=in_hole)

    # Sleeves: each armhole loop is lofted along the upper arm into a deep hanging kimono sleeve.
    for side, c in sides.items():
        ia, ib = c - SIDE_HALF, c + SIDE_HALF
        loop = [vrows[j0][(i) % N] for i in range(ia, ib + 1)]
        loop += [vrows[j][ib % N] for j in range(j0 + 1, top_index + 1)]
        loop += [vrows[top_index][i % N] for i in range(ib - 1, ia - 1, -1)]
        loop += [vrows[j][ia % N] for j in range(top_index - 1, j0, -1)]
        s = SHOULDER[side]
        d = (ELBOW[side] - s).normalized()
        fwd = Vector((0.0, -1.0, 0.0))
        fwd = (fwd - d * fwd.dot(d)).normalized()
        up = fwd.cross(d)
        if up.z < 0:
            up = -up
        centroid = sum((v.co for v in loop), Vector()) / len(loop)
        s0 = (centroid - s).dot(d)
        phis = [math.atan2((v.co - centroid).dot(up), (v.co - centroid).dot(fwd)) for v in loop]
        rows = [loop]
        for k in range(1, SLEEVE_RINGS + 1):
            t = k / SLEEVE_RINGS
            dist = s0 + SLEEVE_LENGTH * t
            w = smoothstep(t / 0.35)
            a = lerp(0.075, 0.105, smoothstep(t / 0.5))
            bb = lerp(0.085, 0.16, smoothstep(t / 0.6))
            drop = lerp(0.0, 0.07, smoothstep(t / 0.6))
            center = s + d * dist - up * drop
            ring = []
            for v, phi in zip(loop, phis):
                ellipse = center + fwd * (a * math.cos(phi)) + up * (bb * math.sin(phi))
                ring.append(b.vert((v.co + d * SLEEVE_LENGTH * t).lerp(ellipse, w), "Kimono", "Sleeve_" + side,
                                   "SleeveRoot_" + side if k <= 3 else "SleeveFree_" + side))
            rows.append(ring)
        # UVs: u around the sleeve, v along it; the last 12 cm carry the black hem band (opening at the band's edge).
        n = len(loop)
        perimeter = 2 * math.pi * 0.13
        sl_uvs = []
        for k in range(SLEEVE_RINGS + 1):
            along = SLEEVE_LENGTH * k / SLEEVE_RINGS
            sl_uvs.append([(i / n * perimeter / IVORY_TILE, along / IVORY_TILE) for i in range(n + 1)])
        border_from = SLEEVE_LENGTH - 0.12

        def sleeve_mat(j, i):
            return slot("Border") if SLEEVE_LENGTH * (j + 0.5) / SLEEVE_RINGS > border_from else slot("Ivory")

        b.grid(rows, sl_uvs, sleeve_mat, closed=True)
        # Border faces need the band's own V: 1 at the opening (wave band) to 0 twelve centimeters up the sleeve.
        for face in b.bm.faces:
            if face.material_index == slot("Border") and all(any(l.vert in r for r in rows[1:]) for l in face.loops):
                for l in face.loops:
                    along = (l.vert.co - s).dot(d) - s0
                    uv = l[b.uv].uv
                    l[b.uv].uv = (uv.x * IVORY_TILE / 0.5, 1.0 - max(0.0, min(1.0, (SLEEVE_LENGTH - along) / 0.12)))
        b.sets.setdefault("Opening_" + side, []).extend(rows[-1])
    # Pins: the torso stays where it is measured (it lies on the body), the sleeves are simulated; the first sleeve
    # rings blend from pinned to free so the seam does not tear.
    b.bm.verts.index_update()
    pins = {v.index: 1.0 for v in vrows_all(vrows)}
    for side in "lr":
        for v in b.sets.get("SleeveRoot_" + side, []):
            pins[v.index] = 0.5
    obj = b.finish("Kimono", ALL_MATS)
    pin = obj.vertex_groups["Pin"]
    pin.remove(list(range(len(obj.data.vertices))))
    for index, w in pins.items():
        pin.add([index], w, "REPLACE")
    push_out(obj, 0.012)
    log("kimono %d vertices, %d faces" % (len(obj.data.vertices), len(obj.data.polygons)))
    return obj, neck


# ---- 3. Robe below the belt (wrap skirt, overlapping in front) ---------------------------------------------------------
SKIRT_TOP = PELVIS.z + 0.07
SKIRT_HEM = 0.33
SKIRT_N = 96


def build_skirt(kimono):
    b = Builder()
    rows = []
    hip_z = PELVIS.z - 0.06
    top_ring = body_ring(SKIRT_TOP, SKIRT_N)
    hip_ring = body_ring(hip_z, SKIRT_N)

    def legs_ring(z):
        """Outermost body (hips and both legs) around the axis at height z - the hands stay out of reach."""
        c = Vector((0.0, axis_y(max(z, PELVIS.z - 0.1)), z))
        out = []
        for i in range(SKIRT_N):
            a = TAU * i / SKIRT_N
            out.append(last_hit(BODY_BVH, c, Vector((math.cos(a), math.sin(a), 0.0)), 0.34) or 0.0)
        return circular_smooth(out, 3)
    rows_z = []
    z = SKIRT_TOP
    while z > SKIRT_HEM + 1e-4:
        rows_z.append(z)
        z -= 0.025
    rows_z.append(SKIRT_HEM)
    overlap = 0.12                                     # fraction of a turn the left panel laps over the right
    count = int(SKIRT_N * (1.0 + overlap))
    start = 0.75 - overlap / 2                         # front center (theta 270 deg) minus half the overlap
    for z in rows_z:
        s = (SKIRT_TOP - z) / (SKIRT_TOP - SKIRT_HEM)
        c = Vector((0.0, axis_y(max(z, PELVIS.z - 0.1)), z))
        clear = legs_ring(z)
        ring = []
        for i in range(count + 1):
            f = start + i / SKIRT_N
            a = TAU * f
            k = int(round(f * SKIRT_N)) % SKIRT_N
            # Width: hips plus ease, flaring into a wide hakama-like hem; a slight box shape.
            base = max(top_ring[k] + 0.026, lerp(top_ring[k] + 0.026, hip_ring[k] + 0.05, smoothstep(s / 0.3)))
            flare = base + 0.17 * smoothstep((s - 0.15) / 0.85) * (1.0 + 0.25 * abs(math.cos(a)))
            flare = max(flare, clear[k] + 0.05 + 0.04 * s)          # always clear of the thighs and knees
            pleat = 1.0 + 0.045 * smoothstep(s / 0.6) * (abs(((f * 16.0) % 1.0) - 0.5) * 2.0 - 0.5)
            r = flare * pleat
            # The outer (left) panel stands off the underlap where they overlap.
            lap = i / SKIRT_N
            if lap > 1.0 - 1e-6:
                r += 0.012 + 0.02 * s
            ring.append(c + Vector((math.cos(a), math.sin(a), 0.0)) * r)
        rows.append(ring)
    rows.reverse()                                     # hem first: v grows upward
    rows_z.reverse()
    vrows = [[b.vert(p, "Skirt") for p in ring] for ring in rows]
    b.sets["Pin"] = vrows[-1] + vrows[-2]
    uvs = []
    band = 0.27
    for j, ring in enumerate(rows):
        u = 0.0
        row = [(0.0, 0.0)]
        for p, q in zip(ring, ring[1:]):
            u += (q - p).length / TILE
            row.append((u, 0.0))
        uvs.append(row)
    for j, z in enumerate(rows_z):
        h = z - SKIRT_HEM
        for i in range(len(uvs[j])):
            uvs[j][i] = (uvs[j][i][0] * TILE / 0.55, (1.0 - h / band) if h <= band + 1e-4 else h / TILE)

    def mat(j, i):
        return slot("Border") if rows_z[j + 1] - SKIRT_HEM <= band + 1e-4 else slot("Motif")

    b.grid(vrows, [r[:-1] + [r[-1]] for r in uvs], mat, closed=False)
    obj = b.finish("Skirt", ALL_MATS)
    push_out(obj, 0.012)
    log("robe %d vertices" % len(obj.data.vertices))
    return obj


# ---- 4. Trousers, bindings, boots, forearm wraps ------------------------------------------------------------------------
def build_trousers():
    b = Builder()
    n = 32
    for side in "lr":
        chain = [(HIP[side] + Vector((0, 0, 0.06)), HIP[side]), (HIP[side], KNEE[side]), (KNEE[side], ANKLE[side])]
        rings = []
        for start, end in chain:
            steps = max(3, int((end - start).length / 0.03))
            for k in range(steps):
                t = k / steps
                center, u, w, radii = limb_ring(start, end, t, n, 0.16)
                z = center.z
                if z < 0.38:
                    ease = lerp(0.006, 0.014, smoothstep((z - 0.2) / 0.18))
                else:
                    ease = 0.045 + 0.015 * math.sin(z * 23.0 + (side == "l"))
                ring = []
                for i in range(n):
                    a = TAU * i / n
                    wrinkle = 0.006 * math.sin(a * 3 + z * 40) * smoothstep((z - 0.36) / 0.1)
                    ring.append(center + (u * math.cos(a) + w * math.sin(a)) * (radii[i] + ease + wrinkle))
                rings.append(ring)
        # Bottom ring tucked into the boot.
        center, u, w, radii = limb_ring(KNEE[side], ANKLE[side], 0.93, n, 0.16)
        rings.append([center + (u * math.cos(TAU * i / n) + w * math.sin(TAU * i / n)) * (radii[i] + 0.006) for i in range(n)])
        rings.reverse()
        tube(b, rings, slot("Plain"), TILE, TILE, tags=("Trousers", "Leg_" + side))
    obj = b.finish("Trousers", ALL_MATS)
    return obj


def ring_curve(name, points, radius, material):
    curve = bpy.data.curves.new(name, "CURVE")
    curve.dimensions = "3D"
    curve.bevel_depth = radius
    curve.bevel_resolution = 2
    spline = curve.splines.new("POLY")
    spline.points.add(len(points) - 1)
    for p, co in zip(spline.points, points):
        p.co = (co.x, co.y, co.z, 1.0)
    spline.use_cyclic_u = True
    obj = bpy.data.objects.new(name, curve)
    scene.collection.objects.link(obj)
    curve.materials.append(material)
    select_only(obj)
    bpy.ops.object.convert(target="MESH")
    obj = bpy.context.view_layer.objects.active
    return obj


def bindings(trousers):
    """Rope turns around the shins and ankles, slightly tilted, lying on the trousers."""
    bvh = evaluated_bvh(trousers)
    parts = []
    for side in "lr":
        for k, z in enumerate((0.215, 0.245, 0.275, 0.305, 0.335, 0.365)):
            pts = []
            tilt = 0.012 * (1 if k % 2 else -1)
            for i in range(24):
                a = TAU * i / 24
                zz = z + tilt * math.sin(a)
                t = (KNEE[side].z - zz) / (KNEE[side].z - ANKLE[side].z)
                c = KNEE[side].lerp(ANKLE[side], t)
                direction = Vector((math.cos(a), math.sin(a), 0.0))
                d = last_hit(bvh, Vector((c.x, c.y, zz)), direction, 0.2) or 0.06
                pts.append(Vector((c.x, c.y, zz)) + direction * (d + 0.004))
            parts.append(ring_curve("Rope_%s_%d" % (side, k), pts, 0.0045, MAT["Rope"]))
    return parts


def build_boots():
    """A box around each foot, shrink-wrapped onto it, smoothed into a boot; flat sole."""
    boots = []
    for side in "lr":
        verts = [v.co for v in body.data.vertices if v.co.z < 0.14 and (v.co.x > 0) == (side == "l")]
        lo = Vector((min(p.x for p in verts), min(p.y for p in verts), 0.0))
        hi = Vector((max(p.x for p in verts), max(p.y for p in verts), 0.0))
        bpy.ops.mesh.primitive_cube_add(size=1.0)
        boot = bpy.context.view_layer.objects.active
        boot.name = "Boot_" + side
        boot.scale = ((hi.x - lo.x) + 0.05, (hi.y - lo.y) + 0.05, 0.215)
        boot.location = ((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, 0.1025)
        bpy.ops.object.transform_apply(location=True, scale=True, rotation=False)
        bm = bmesh.new()
        bm.from_mesh(boot.data)
        top = [f for f in bm.faces if f.normal.z > 0.9]
        bmesh.ops.delete(bm, geom=top, context="FACES")
        bm.to_mesh(boot.data)
        bm.free()
        sub = boot.modifiers.new("Sub", "SUBSURF")
        sub.levels = 4
        sub.subdivision_type = "SIMPLE"
        wrap = boot.modifiers.new("Wrap", "SHRINKWRAP")
        wrap.target = body
        wrap.wrap_method = "NEAREST_SURFACEPOINT"
        wrap.offset = 0.02
        smooth = boot.modifiers.new("Smooth", "SMOOTH")
        smooth.factor = 0.8
        smooth.iterations = 12
        select_only(boot)
        for mod in list(boot.modifiers):
            bpy.ops.object.modifier_apply(modifier=mod.name)
        for v in boot.data.vertices:
            if v.co.z < 0.012:
                v.co.z = -0.004
        boot.data.materials.append(MAT["Boot"])
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.cube_project(cube_size=0.3)
        bpy.ops.object.mode_set(mode="OBJECT")
        for poly in boot.data.polygons:
            poly.use_smooth = True
        boots.append(boot)
        # Rope ties over the instep and around the ankle.
        bvh = evaluated_bvh(boot)
        for k, z in enumerate((0.075, 0.15)):
            pts = []
            c = ANKLE[side] + (BALL[side] - ANKLE[side]) * (0.35 if k == 0 else 0.0)
            for i in range(24):
                a = TAU * i / 24
                direction = Vector((math.cos(a), math.sin(a), 0.0))
                zz = z + (0.03 * -math.sin(a) if k == 0 else 0.0)
                d = last_hit(bvh, Vector((c.x, c.y, zz)), direction, 0.2) or 0.06
                pts.append(Vector((c.x, c.y, zz)) + direction * (d + 0.003))
            boots.append(ring_curve("BootTie_%s_%d" % (side, k), pts, 0.004, MAT["Rope"]))
    return boots


def build_arm_wraps():
    b = Builder()
    n = 28
    for side in "lr":
        start = WRIST[side] - (WRIST[side] - ELBOW[side]).normalized() * 0.2
        end = WRIST[side] + (WRIST[side] - ELBOW[side]).normalized() * 0.015
        rings = []
        for k in range(11):
            t = k / 10
            center, u, w, radii = limb_ring(start, end, t, n, 0.1)
            bulge = 0.004 * math.sin(t * math.pi * 10) ** 2
            rings.append([center + (u * math.cos(TAU * i / n) + w * math.sin(TAU * i / n)) * (radii[i] + 0.007 + bulge)
                          for i in range(n)])
        tube(b, rings, slot("Plain"), 0.25, 0.25, tags=("ArmWrap", "Wrap_" + side))
    return b.finish("ArmWraps", ALL_MATS)


# ---- 5. Belts, collar, scarf, straps, hanging pieces --------------------------------------------------------------------
def band(name, bvh, z_mid, height, offset, material, tilt=0.0, n=96, rows=4, tags=()):
    b = Builder()
    rings = []
    for k in range(rows + 1):
        ring = []
        for i in range(n):
            a = TAU * i / n
            z = z_mid - height / 2 + height * k / rows + tilt * math.sin(a)
            direction = Vector((math.cos(a), math.sin(a), 0.0))
            c = Vector((0.0, axis_y(z), z))
            d = last_hit(bvh, c, direction, 0.5) or 0.15
            ring.append(c + direction * (d + offset))
        rings.append(ring)
    tube(b, rings, material, 0.3, 0.3, tags=tags)
    return b.finish(name, ALL_MATS)


def ribbon(name, path, bvh, width, offset, material, tags=(), v_tile=0.3, project=None):
    """A strip lying on the surface along the path: each point projected onto the nearest surface, or - project
    "front"/"back" - onto the surface seen from the front or back (a path across the chest stays on the chest)."""
    dense = []
    for p, q in zip(path, path[1:]):
        steps = max(1, int((q - p).length / 0.012))
        dense += [p.lerp(q, k / steps) for k in range(steps)]
    dense.append(path[-1])
    on = []
    for p in dense:
        hit = None
        if project in ("front", "back"):
            sign = 1.0 if project == "front" else -1.0
            h = bvh.ray_cast(Vector((p.x, -0.8 * sign, p.z)), Vector((0.0, sign, 0.0)), 1.6)
            hit = (h[0], h[1]) if h[0] is not None else None
        if hit is None:
            loc, normal, _, _ = bvh.find_nearest(p)
            hit = (loc, normal) if loc is not None else (p, Vector((0, -1, 0)))
        on.append((hit[0] + hit[1] * offset, hit[1]))
    for _ in range(3):
        on = [on[0]] + [((on[i - 1][0] + on[i][0] * 2 + on[i + 1][0]) / 4,
                         (on[i - 1][1] + on[i][1] + on[i + 1][1]).normalized()) for i in range(1, len(on) - 1)] + [on[-1]]
    b = Builder()
    left, right, uvl, uvr = [], [], [], []
    v = 0.0
    previous = None
    for i, (p, normal) in enumerate(on):
        tangent = (on[min(i + 1, len(on) - 1)][0] - on[max(i - 1, 0)][0]).normalized()
        side = normal.cross(tangent).normalized()
        if previous is not None and side.dot(previous) < 0.0:
            side = -side                                   # no twists where the surface normal turns over
        previous = side
        if i > 0:
            v += (p - on[i - 1][0]).length / v_tile
        left.append(b.vert(p - side * width / 2, *tags))
        right.append(b.vert(p + side * width / 2, *tags))
        uvl.append((0.0, v))
        uvr.append((width / v_tile, v))
    for i in range(len(on) - 1):
        b.quad((left[i], right[i], right[i + 1], left[i + 1]), (uvl[i], uvr[i], uvr[i + 1], uvl[i + 1]), material)
    return b.finish(name, ALL_MATS)


def surface_point(bvh, x, z, front=True, offset=0.0):
    """Point on the outer surface seen from the front (or back) at (x, z)."""
    y0 = -0.6 if front else 0.6
    hit = bvh.ray_cast(Vector((x, y0, z)), Vector((0, 1 if front else -1, 0)), 1.2)
    if hit[0] is None:
        return Vector((x, axis_y(z) + (-0.1 if front else 0.1), z))
    return hit[0] + hit[1] * offset


def build_scarf(bvh):
    """Thick black hood scarf: a wide cowl pinned around the neck, cut longer than the way down to the shoulders, so the
    simulation piles the surplus into heavy folds on the shoulders - high and bunched at the nape (the hood lying
    down), lower in front where it falls onto the chest."""
    b = Builder()
    n, k_rows = 72, 16
    rings = []
    for k in range(k_rows + 1):
        t = k / k_rows
        ring = []
        for i in range(n):
            a = TAU * i / n
            back = 0.5 + 0.5 * math.sin(a)                  # 0 in front, 1 at the nape
            direction = Vector((math.cos(a), math.sin(a), 0.0))
            z_top = NECK.z + 0.05 + 0.05 * back
            z_bot = SHOULDER["l"].z - lerp(0.17, 0.05, back)
            c_top = Vector((0.0, axis_y(z_top), z_top))
            c_bot = Vector((0.0, axis_y(z_bot), z_bot))
            r_top = (first_hit(BODY_BVH, c_top, direction, 0.2) or 0.07) + 0.035
            r_bot = min((last_hit(bvh, c_bot, direction, 0.4) or 0.2) + 0.035, 0.3)
            bulge = (0.08 + 0.04 * back) * math.sin(math.pi * t) + 0.012 * math.sin(5 * a + 3 * t)
            c = c_top.lerp(c_bot, t)
            ring.append(c + direction * (lerp(r_top, r_bot, t) + bulge))
        rings.append(ring)
    rows = tube(b, rings, slot("Plain"), TILE, TILE, tags=("Scarf",))
    b.sets["Pin"] = list(rows[0])
    obj = b.finish("Scarf", ALL_MATS)
    push_out(obj, 0.012)
    return obj


def build_stole(bvh):
    """The black stole over the left shoulder: laid onto the coat (front, over the shoulder, down the back), then
    simulated so it falls into folds."""
    x0, width, cols = SHOULDER["l"].x * 0.62, 0.22, 12
    xs = [x0 - width / 2 + width * c / cols for c in range(cols + 1)]
    top_z = SHOULDER["l"].z - 0.03

    def cast(origin, direction):
        h = bvh.ray_cast(origin, direction, 3.0)
        return h[0] + h[1] * 0.014 if h[0] is not None else None

    rows = []
    for k in range(6):                                             # in front only over the collarbone
        z = lerp(top_z - 0.16, top_z, k / 5)
        rows.append([cast(Vector((x, -0.8, z)), Vector((0, 1, 0))) for x in xs])
    front = [p for p in rows[-1] if p]
    back = [p for p in (cast(Vector((x, 0.8, top_z)), Vector((0, -1, 0))) for x in xs) if p]
    front_y = sum(p.y for p in front) / max(1, len(front))
    back_y = sum(p.y for p in back) / max(1, len(back))
    for k in range(1, 8):                                          # over the shoulder
        y = lerp(front_y, back_y, k / 8)
        rows.append([cast(Vector((x, y, 2.2)), Vector((0, 0, -1))) for x in xs])
    for k in range(20):                                            # down the back
        z = lerp(top_z, PELVIS.z - 0.02, k / 19)
        rows.append([cast(Vector((x, 0.8, z)), Vector((0, -1, 0))) for x in xs])
    for k, row in enumerate(rows):                                 # a missed ray: continue the neighbour sideways
        for c in range(len(row)):
            if row[c] is None:
                near = sorted((abs(c2 - c), c2) for c2 in range(len(row)) if row[c2] is not None)
                if near:
                    row[c] = row[near[0][1]] + Vector((xs[c] - xs[near[0][1]], 0.0, 0.0))
                else:
                    row[c] = Vector((xs[c], 0.0, top_z + 0.05)) if k < len(rows) else None
    rows = [row for row in rows if all(p is not None for p in row)]
    b = Builder()
    vrows = [[b.vert(p, "Stole") for p in row] for row in rows]
    uvs, v = [], 0.0
    for k, row in enumerate(rows):
        if k:
            v += (row[cols // 2] - rows[k - 1][cols // 2]).length / TILE
        uvs.append([(c * width / cols / TILE, v) for c in range(cols + 1)])
    b.grid(vrows, uvs, slot("Motif"), closed=False)
    b.sets["Pin"] = [p for row in vrows[3:15] for p in row]        # the part on the shoulder
    return b.finish("Stole", ALL_MATS)


def build_hanging(bvh):
    """Front: a leather strap with a ring and a cream tassel on the belt; back: a second strap. Plus belt buckle."""
    parts = []
    for front, x, length in ((True, 0.06, 0.36), (False, -0.05, 0.30)):
        top = surface_point(bvh, x, PELVIS.z + 0.04, front, 0.012)
        bottom = top + Vector((0.0, -0.035 if front else 0.035, -length))
        strap = ribbon("Strap_%s" % ("front" if front else "back"), [top, top.lerp(bottom, 0.5), bottom],
                       bvh, 0.055, 0.012, slot("Leather"), tags=("Hanging",), project="front" if front else "back")
        parts.append(strap)
        bpy.ops.mesh.primitive_torus_add(major_radius=0.028, minor_radius=0.005, major_segments=24, minor_segments=8,
                                         location=bottom + Vector((0, -0.01 if front else 0.01, -0.02)),
                                         rotation=(math.pi / 2, 0, 0))
        ring = bpy.context.view_layer.objects.active
        ring.name = "Ring_%s" % ("front" if front else "back")
        ring.data.materials.append(MAT["Metal"])
        parts.append(ring)
    # Tassel: a knot and a skirt of cords.
    top = surface_point(bvh, -0.07, PELVIS.z + 0.02, True, 0.02)
    b = Builder()
    for k in range(40):
        a = TAU * k / 40
        start = top + Vector((math.cos(a) * 0.012, math.sin(a) * 0.012, -0.035))
        end = top + Vector((math.cos(a) * 0.03 + random.uniform(-0.005, 0.005), math.sin(a) * 0.03 - 0.01,
                            -0.2 - random.uniform(0.0, 0.03)))
        rings = []
        for t in (0.0, 1.0):
            c = start.lerp(end, t)
            rings.append([c + Vector((math.cos(TAU * i / 4) * 0.0022, math.sin(TAU * i / 4) * 0.0022, 0.0)) for i in range(4)])
        tube(b, rings, slot("Tassel"), 0.05, 0.2, tags=("Hanging",))
    parts.append(b.finish("Tassel", ALL_MATS))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.016, segments=12, ring_count=8, location=top + Vector((0, -0.004, -0.03)))
    knot = bpy.context.view_layer.objects.active
    knot.name = "TasselKnot"
    knot.scale = (1.0, 1.0, 1.4)
    knot.data.materials.append(MAT["Tassel"])
    parts.append(knot)
    cord = ribbon("TasselCord", [top + Vector((0, 0, 0.03)), top + Vector((0, -0.005, -0.03))], bvh, 0.01, 0.014,
                  slot("Tassel"), tags=("Hanging",))
    parts.append(cord)
    # Buckle on the leather belt.
    p = surface_point(bvh, 0.0, PELVIS.z + 0.065, True, 0.012)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.034, minor_radius=0.0055, major_segments=4, minor_segments=8,
                                     location=p + Vector((0, -0.004, 0)), rotation=(math.pi / 2, math.pi / 4, 0))
    buckle = bpy.context.view_layer.objects.active
    buckle.name = "Buckle"
    buckle.scale = (1.25, 1.0, 1.0)
    buckle.data.materials.append(MAT["Metal"])
    parts.append(buckle)
    return parts


# ---- 6. Weights --------------------------------------------------------------------------------------------------------
DEFORM = [b.name for b in armature.data.bones if not (b.name.startswith("ik_") or b.name in ("root", "interaction", "center_of_mass"))]
SEGMENTS = {n: (bone(n), bone(n, True)) for n in DEFORM}


def segment_distance(point, head, tail):
    axis = tail - head
    length = axis.length_squared
    t = 0.0 if length == 0.0 else max(0.0, min(1.0, (point - head).dot(axis) / length))
    return (point - (head + axis * t)).length


def rule_weights(point, allowed, robe=False):
    scored = sorted((max(segment_distance(point, *SEGMENTS[n]), 0.01), n) for n in allowed if n in SEGMENTS)[:4]
    weights = {n: 1.0 / d ** 6 for d, n in scored}
    total = sum(weights.values())
    weights = {n: w / total for n, w in weights.items()}
    if robe:
        legs = [n for n in weights if n.startswith(("thigh", "calf"))]
        if legs:
            gap = min(segment_distance(point, *SEGMENTS[n]) for n in ("thigh_l", "thigh_r", "calf_l", "calf_r"))
            share = max(0.0, min(0.6, (gap - 0.05) / 0.15))
            weights = {n: w * (1.0 - share) for n, w in weights.items()}
            weights["pelvis"] = weights.get("pelvis", 0.0) + share
    return weights


ARM = {s: ["clavicle_" + s, "upperarm_" + s, "lowerarm_" + s] for s in "lr"}
ROBE = ["pelvis", "spine_01", "thigh_l", "thigh_r", "calf_l", "calf_r"]


def skin(obj, rule=None, blend_group=None):
    """Body weights by transfer (nearest face, interpolated); where rule(vertex) gives a bone set, distance weights are
    blended in by the vertex's weight in blend_group (1 = rules only)."""
    select_only(obj, body)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.data_transfer(use_reverse_transfer=False, data_type="VGROUP_WEIGHTS", vert_mapping="POLYINTERP_NEAREST",
                                 layers_select_src="ALL", layers_select_dst="NAME")
    if rule:
        bones = {g.index: g.name for g in obj.vertex_groups}
        groups = {g.name: g for g in obj.vertex_groups}
        blend = obj.vertex_groups.get(blend_group) if blend_group else None
        for v in obj.data.vertices:
            spec = rule(v)
            if not spec:
                continue
            allowed, robe = spec
            f = 1.0
            if blend:
                f = next((g.weight for g in v.groups if g.group == blend.index), 0.0)
            if f <= 0.0:
                continue
            current = {bones[g.group]: g.weight for g in v.groups if bones[g.group] in SEGMENTS}
            total = sum(current.values()) or 1.0
            new = rule_weights(v.co, allowed, robe)
            mixed = {n: (1 - f) * current.get(n, 0.0) / total + f * new.get(n, 0.0) for n in set(current) | set(new)}
            for n, w in mixed.items():
                if n not in groups:
                    groups[n] = obj.vertex_groups.new(name=n)
                    bones[groups[n].index] = n
                if w > 0.01:
                    groups[n].add([v.index], w, "REPLACE")
                elif n in current:
                    groups[n].remove([v.index])


# ---- Check renders -----------------------------------------------------------------------------------------------------
def render_views(prefix, views=("front", "side", "back")):
    body.modifiers.clear()
    if "Skin" not in bpy.data.materials:
        skin_mat = plain_material("Skin", (0.45, 0.30, 0.22), 0.55)
        body.data.materials.clear()
        body.data.materials.append(skin_mat)
        scene.render.engine = "BLENDER_EEVEE"
        scene.render.resolution_x = 700
        scene.render.resolution_y = 1150
        scene.view_settings.view_transform = "AgX"
        world = bpy.data.worlds.new("Check")
        world.use_nodes = True
        world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.32, 0.32, 0.33, 1.0)
        world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.7
        scene.world = world
        for energy, rot in ((3.5, (math.radians(50), 0, math.radians(-30))), (1.5, (math.radians(60), 0, math.radians(150)))):
            light = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
            light.data.energy = energy
            light.rotation_euler = rot
            scene.collection.objects.link(light)
        cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
        cam.data.type = "ORTHO"
        cam.data.ortho_scale = 2.0
        scene.collection.objects.link(cam)
        scene.camera = cam
    cam = scene.camera
    placements = {"front": ((0, -4, 0.95), (math.pi / 2, 0, 0)), "side": ((4, 0, 0.95), (math.pi / 2, 0, math.pi / 2)),
                  "back": ((0, 4, 0.95), (math.pi / 2, 0, math.pi)),
                  "three": ((2.6, -3.0, 1.3), (math.radians(82), 0, math.radians(41)))}
    for view in views:
        cam.location, cam.rotation_euler = placements[view]
        scene.render.filepath = os.path.join(OUT, "%s_%s.png" % (prefix, view))
        bpy.ops.render.render(write_still=True)
        log("render " + scene.render.filepath)


# ---- 7. Build ----------------------------------------------------------------------------------------------------------
make_collider(body, 0.006)
kimono, neck_ring = build_kimono()
if STAGE == "kimono_raw":
    render_views("stage_kimono_raw", ("front", "side", "three"))
    raise SystemExit
drop_cloth(kimono, 30, pin="Pin", mass=0.3, bending=3.0)
if STAGE == "kimono":
    render_views("stage_kimono", ("front", "side", "three"))
    raise SystemExit
make_collider(kimono, 0.004)
skirt = build_skirt(kimono)
drop_cloth(skirt, 45, pin="Pin", mass=0.4, bending=1.5)
if STAGE == "lower":
    render_views("stage_lower", ("front", "side", "three"))
    raise SystemExit
trousers = build_trousers()
ropes = bindings(trousers)
boots = build_boots()
wraps = build_arm_wraps()

outer = evaluated_bvh(kimono, skirt)
obi = band("Obi", outer, PELVIS.z + 0.065, 0.135, 0.006, slot("Plain"), tags=("Belt",))
belt_bvh = evaluated_bvh(kimono, skirt, obi)
belt = band("Belt", belt_bvh, PELVIS.z + 0.065, 0.055, 0.005, slot("Leather"), tags=("Belt",))
belt2 = band("Belt2", belt_bvh, PELVIS.z + 0.0, 0.032, 0.008, slot("Leather"), tilt=0.02, tags=("Belt",))
# The stole lies on the coat and goes under the scarf, which then falls over both.
stole = build_stole(evaluated_bvh(kimono))
drop_cloth(stole, 35, pin="Pin", mass=0.3, bending=0.6)
make_collider(stole, 0.004)
scarf = build_scarf(evaluated_bvh(kimono, skirt, stole))
drop_cloth(scarf, 40, pin="Pin", mass=0.5, bending=4.0, self_collision=True)
make_collider(scarf, 0.004)

# Collar: the kimono's black band edges, left over right - from under the scarf down the chest; the right panel's edge
# disappears under the left one at the breastbone, the left one runs on into the obi.
coat = evaluated_bvh(kimono)
collar = []
for name, start_x, end_x, end_z, offset in (("CollarUnder", -0.075, 0.03, CHEST - 0.03, 0.004),
                                            ("CollarOver", 0.075, -0.11, BELT_HIGH, 0.008)):
    start = Vector((start_x, 0.0, NECK.z - 0.05))
    end = Vector((end_x, 0.0, end_z))
    collar.append(ribbon(name, [start, start.lerp(end, 0.5), end], coat, 0.045, offset, slot("Plain"),
                         tags=("Collar",), project="front"))
strap_bvh = evaluated_bvh(kimono, scarf, stole)
bandolier = ribbon("Bandolier", [surface_point(strap_bvh, SHOULDER["r"].x * 0.5, CHEST + 0.06, True, 0.0),
                                 surface_point(strap_bvh, 0.0, CHEST - 0.08, True, 0.0),
                                 surface_point(strap_bvh, 0.13, BELT_HIGH + 0.01, True, 0.0)],
                   strap_bvh, 0.04, 0.008, slot("Leather"), tags=("Strap",), project="front")
hanging = build_hanging(evaluated_bvh(skirt, obi, belt))

# The scarf and stole fold tightly: one subdivision step before their thickness.
for obj in (scarf, stole):
    sub = obj.modifiers.new("Sub", "SUBSURF")
    sub.levels = 1
    sub.render_levels = 1
# Thickness: cloth gets real edges, belts and straps a few millimeters of leather.
for obj, thickness in ((kimono, 0.004), (skirt, 0.004), (stole, 0.006), (scarf, 0.009), (obi, 0.004), (belt, 0.005), (belt2, 0.004),
                       (bandolier, 0.004), *[(c, 0.003) for c in collar]):
    sol = obj.modifiers.new("Solidify", "SOLIDIFY")
    sol.thickness = thickness
    sol.offset = 1.0
    sol.use_quality_normals = True
for part in hanging:
    if part.name.startswith("Strap"):
        sol = part.modifiers.new("Solidify", "SOLIDIFY")
        sol.thickness = 0.004
        sol.offset = 1.0

# Weights, then one mesh.
skin(kimono, lambda v: (ARM["l"] + ["spine_05"], False) if v.co.x > 0 else (ARM["r"] + ["spine_05"], False),
     blend_group="SleeveFree_l")
skin(kimono, lambda v: (ARM["r"] + ["spine_05"], False) if v.co.x < 0 else None, blend_group="SleeveFree_r")
skin(skirt, lambda v: (ROBE, True))
skin(stole, lambda v: (["spine_03", "spine_04", "spine_05", "clavicle_l", "pelvis", "spine_01", "spine_02"], False)
     if v.co.z < SHOULDER["l"].z - 0.1 else None)
for obj in [trousers, wraps, obi, belt, belt2, scarf, bandolier] + ropes + boots + collar:
    skin(obj)
for part in hanging:
    skin(part, lambda v: (["pelvis", "thigh_l", "thigh_r"], True))

parts = [kimono, skirt, stole, trousers, wraps, obi, belt, belt2, scarf, bandolier] + ropes + boots + collar + hanging
hard_parts = {obi, belt, belt2, bandolier, *collar, *hanging}   # thin strips: sharp rims, or they read as tubes
for obj in parts:
    select_only(obj)
    for mod in [m for m in obj.modifiers if m.type != "COLLISION"]:
        bpy.ops.object.modifier_apply(modifier=mod.name)
    for mod in [m for m in obj.modifiers if m.type == "COLLISION"]:
        obj.modifiers.remove(mod)
    if obj in hard_parts:
        bpy.ops.object.shade_smooth_by_angle(angle=math.radians(40))
    # Materials by name into the shared slot order.
    slots = [ALL_MATS.index(m) if m in ALL_MATS else 0 for m in obj.data.materials] or [0]
    indices = [slots[min(p.material_index, len(slots) - 1)] for p in obj.data.polygons]
    obj.data.materials.clear()
    for mat in ALL_MATS:
        obj.data.materials.append(mat)
    obj.data.polygons.foreach_set("material_index", indices)
select_only(*parts)
bpy.context.view_layer.objects.active = kimono
bpy.ops.object.join()
outfit = kimono
outfit.name = "SK_Akaza_Outfit"
for group in [g for g in outfit.vertex_groups if g.name not in SEGMENTS]:
    outfit.vertex_groups.remove(group)
select_only(outfit)
bpy.ops.object.vertex_group_limit_total(limit=4)
bpy.ops.object.vertex_group_normalize_all(lock_active=False)
unweighted = sum(1 for v in outfit.data.vertices if not v.groups)
log("outfit %d vertices, %d triangles, %d unweighted" % (len(outfit.data.vertices),
    sum(len(p.vertices) - 2 for p in outfit.data.polygons), unweighted))
outfit.parent = armature
mod = outfit.modifiers.new("Armature", "ARMATURE")
mod.object = armature

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "Akaza_Outfit.blend"))


def rescale(factor):
    """Scales armature and outfit about the origin and applies it (the outfit unparented meanwhile, so its bind stays)."""
    outfit.parent = None
    for obj in (armature, outfit):
        obj.scale = (factor, factor, factor)
    select_only(armature, outfit)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    outfit.parent = armature
    outfit.matrix_parent_inverse = Matrix.Identity(4)


# Export in centimetres, as Unreal works: mesh and bones carry the same numbers. With metres and "FBX units scale"
# the mesh came in as centimetres but the bones stayed in metres - the outfit then skinned to nothing in the game.
rescale(100.0)
scene.unit_settings.system = "METRIC"
scene.unit_settings.scale_length = 0.01
select_only(armature, outfit)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "SK_Akaza_Outfit.fbx"), use_selection=True, global_scale=1.0,
                         apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", add_leaf_bones=False, bake_anim=False,
                         mesh_smooth_type="FACE", path_mode="STRIP", embed_textures=False, use_armature_deform_only=False,
                         primary_bone_axis="Y", secondary_bone_axis="X", armature_nodetype="NULL")
log("exported %s (pelvis at %.1f cm)" % (os.path.join(OUT, "SK_Akaza_Outfit.fbx"), armature.data.bones["pelvis"].head_local.z))
rescale(0.01)
scene.unit_settings.scale_length = 1.0


render_views("Akaza_Outfit_check", ("front", "side", "back", "three"))
