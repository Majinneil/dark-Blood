# Run inside the Unreal Editor Python environment (UE 5.8):
#   UnrealEditor-Cmd DarkBlood.uproject -run=pythonscript -script=Tools/UE58/db_measure_ship_decks.py
#
# Measures the walkable deck of the ship models (FDBShipSpec::ModelDeckZ): places each model like
# DBModels::BuildScaledToLength (turn, scale to length, keel at -draft) and casts vertical rays through its triangles.
# DBDECKP lists every surface per probe; amidships the only surface above the water line is the main deck.
import unreal, math
# key, yaw, length, draft, exclude
SHIPS = [("junk_red_large", -90, 4800, 350, None), ("junk_red_small", 0, 3400, 250, None),
         ("junk_merchant", -90, 3800, 300, "Material_006"), ("wooden_boat", -90, 900, 5, None)]
ROOT = "/Game/DarkBlood/Art/Environment/Sketchfab/"

def rot(v, yaw):
    a = math.radians(yaw); c, s = math.cos(a), math.sin(a)
    return (v[0] * c - v[1] * s, v[0] * s + v[1] * c, v[2])

for key, yaw, length, draft, exclude in SHIPS:
    tris = []
    for path in unreal.EditorAssetLibrary.list_assets(ROOT + key, recursive=True, include_folder=False):
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh) or (exclude and exclude in mesh.get_name()):
            continue
        desc = mesh.get_static_mesh_description(0)
        for t in range(desc.get_triangle_count()):
            tid = unreal.TriangleID(t)
            vis = desc.get_triangle_vertex_instances(tid)
            pts = []
            for vi in vis:
                p = desc.get_vertex_position(desc.get_vertex_instance_vertex(vi))
                pts.append(rot((p.x, p.y, p.z), yaw))
            tris.append(pts)
    xs = [p[0] for t in tris for p in t]; ys = [p[1] for t in tris for p in t]; zs = [p[2] for t in tris for p in t]
    lo = (min(xs), min(ys), min(zs)); hi = (max(xs), max(ys), max(zs))
    scale = length / (hi[0] - lo[0]); cx = (lo[0] + hi[0]) / 2; cy = (lo[1] + hi[1]) / 2
    def world(p):
        return ((p[0] - cx) * scale, (p[1] - cy) * scale, (p[2] - lo[2]) * scale - draft)
    wt = [[world(p) for p in t] for t in tris]
    results = []
    for f in (-0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3):
        for off in (-0.12, 0.12):
            px, py = f * length, off * length * 0.25
            best = None
            hits = []
            for (a, b, c) in wt:
                # point in triangle (xy), barycentric
                d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
                if abs(d) < 1e-6:
                    continue
                l1 = ((b[1] - c[1]) * (px - c[0]) + (c[0] - b[0]) * (py - c[1])) / d
                l2 = ((c[1] - a[1]) * (px - c[0]) + (a[0] - c[0]) * (py - c[1])) / d
                l3 = 1 - l1 - l2
                if l1 < 0 or l2 < 0 or l3 < 0:
                    continue
                z = l1 * a[2] + l2 * b[2] + l3 * c[2]
                if z > 20:
                    hits.append(z)
                if z > 20 and (best is None or z < best):
                    best = z
            if best is not None:
                results.append(best)
            if off > 0:
                unreal.log("DBDECKP %s x=%.0f hits=%s" % (key, px, ",".join("%.0f" % h for h in sorted(set(round(h / 10) * 10 for h in hits))[:14])))
    results.sort()
    unreal.log("DBDECK %s probes=%d median=%.0f all=%s" % (key, len(results), results[len(results) // 2] if results else -1,
                                                       ",".join("%.0f" % r for r in results)))
