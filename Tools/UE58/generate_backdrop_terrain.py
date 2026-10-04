# Generates the DARK BLOOD horizon terrain as OBJ meshes (plain Python 3, no dependencies):
#   python Tools/UE58/generate_backdrop_terrain.py
# -> SourceArt/Generated/SM_DB_Backdrop_Mountains.obj   ring 3.5-14 km, ridged peaks up to ~3 km, snow above the snow line
#    SourceArt/Generated/SM_DB_Backdrop_Hills.obj       ring 0.55-4 km, rolling hills up to ~220 m
# Both open towards the coast in the south (local -Y). Units: cm, Z up, centered on the slice center.
# Import with Tools/UE58/db_import_backdrop_terrain.py (Nanite, materials per slot). Deterministic (fixed seed).
import math
import pathlib
import random

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "SourceArt" / "Generated"

# ---- gradient noise --------------------------------------------------------------------------------------------
_rng = random.Random(1709)
_perm = list(range(256))
_rng.shuffle(_perm)
_perm += _perm
_grad = [(math.cos(a), math.sin(a)) for a in (i * 2.0 * math.pi / 16.0 for i in range(16))]


def _fade(t):
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)


def noise(x, y):
    """Perlin gradient noise in [-1, 1]."""
    xi, yi = int(math.floor(x)), int(math.floor(y))
    xf, yf = x - xi, y - yi
    xi &= 255
    yi &= 255

    def dot(ix, iy, dx, dy):
        g = _grad[_perm[_perm[ix] + iy] & 15]
        return g[0] * dx + g[1] * dy

    u, v = _fade(xf), _fade(yf)
    a = dot(xi, yi, xf, yf) + u * (dot(xi + 1, yi, xf - 1.0, yf) - dot(xi, yi, xf, yf))
    b = dot(xi, yi + 1, xf, yf - 1.0) + u * (dot(xi + 1, yi + 1, xf - 1.0, yf - 1.0) - dot(xi, yi + 1, xf, yf - 1.0))
    return (a + v * (b - a)) * 1.41


def fbm(x, y, octaves=5, lacunarity=2.03, gain=0.5):
    total, amplitude, frequency = 0.0, 1.0, 1.0
    for _ in range(octaves):
        total += noise(x * frequency, y * frequency) * amplitude
        frequency *= lacunarity
        amplitude *= gain
    return total


def ridged(x, y, octaves=7):
    """Ridged multifractal in ~[0, 1]: sharp crests, eroded-looking valleys."""
    total, weight, frequency, amplitude, norm = 0.0, 1.0, 1.0, 0.5, 0.0
    for _ in range(octaves):
        signal = 1.0 - abs(noise(x * frequency, y * frequency))
        signal *= signal * weight
        weight = min(1.0, max(0.0, signal * 1.8))
        total += signal * amplitude
        norm += amplitude
        frequency *= 2.1
        amplitude *= 0.52
    return total / norm


def smoothstep(e0, e1, x):
    t = min(1.0, max(0.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


def gap_factor(theta, center_deg=-90.0, half_width_deg=26.0, soft_deg=18.0):
    delta = abs((math.degrees(theta) - center_deg + 180.0) % 360.0 - 180.0)
    return smoothstep(half_width_deg, half_width_deg + soft_deg, delta)


# ---- ring meshes ----------------------------------------------------------------------------------------------
def ring(name, r_in, r_out, segments, rings, height, slot_of):
    """Polar grid ring; height(r, theta) -> cm; slot_of(z, normal_z) -> material slot name."""
    verts = []
    radii = [r_in * (r_out / r_in) ** (j / (rings - 1)) for j in range(rings)]  # denser near the viewer
    for j, r in enumerate(radii):
        for i in range(segments):
            theta = 2.0 * math.pi * i / segments
            verts.append((r * math.cos(theta), r * math.sin(theta), height(r, theta)))
    faces = {}
    normals = [[0.0, 0.0, 0.0] for _ in verts]
    for j in range(rings - 1):
        for i in range(segments):
            a = j * segments + i
            b = j * segments + (i + 1) % segments
            c = (j + 1) * segments + (i + 1) % segments
            d = (j + 1) * segments + i
            for tri in ((a, c, b), (a, d, c)):
                p0, p1, p2 = verts[tri[0]], verts[tri[1]], verts[tri[2]]
                ux, uy, uz = p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]
                vx, vy, vz = p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]
                nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
                length = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
                slot = slot_of((p0[2] + p1[2] + p2[2]) / 3.0, abs(nz) / length)
                faces.setdefault(slot, []).append(tri)
                for index in tri:  # area-weighted smooth vertex normals
                    normals[index][0] += nx
                    normals[index][1] += ny
                    normals[index][2] += nz
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / (name + ".obj")
    with path.open("w", encoding="ascii") as f:
        f.write("# DARK BLOOD generated backdrop terrain (generate_backdrop_terrain.py)\no %s\n" % name)
        for x, y, z in verts:
            f.write("v %.1f %.1f %.1f\n" % (x, y, z))
        for x, y, z in verts:  # planar UVs, 1 unit = 1 km (the terrain materials are world-aligned)
            f.write("vt %.5f %.5f\n" % (x / KM, y / KM))
        for nx, ny, nz in normals:
            length = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
            f.write("vn %.4f %.4f %.4f\n" % (nx / length, ny / length, nz / length))
        for slot, tris in sorted(faces.items()):
            f.write("usemtl %s\n" % slot)  # material sections of one object (a "g" group would become its own mesh)
            for a, b, c in tris:
                f.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a + 1, a + 1, a + 1, b + 1, b + 1, b + 1, c + 1, c + 1, c + 1))
    print("%s: %d vertices, %d triangles (%s)" % (path.name, len(verts), sum(len(t) for t in faces.values()),
                                                 ", ".join("%s %d" % (k, len(v)) for k, v in sorted(faces.items()))))


KM = 100000.0


def mountains(r, theta):
    # Few massive peaks instead of a saw blade: broad ridged base, fine detail only on the crests.
    x, y = r * math.cos(theta) / (6.5 * KM), r * math.sin(theta) / (6.5 * KM)
    rise = smoothstep(3.5 * KM, 7.0 * KM, r) * (1.0 - 0.55 * smoothstep(11.5 * KM, 14.0 * KM, r))
    massif = min(1.0, max(0.0, fbm(x * 0.45 + 7.1, y * 0.45 - 3.3, 3) * 0.6 + 0.55))
    peaks = ridged(x, y, octaves=6)
    h = rise * (0.35 + 0.65 * massif ** 1.5) * (0.3 * KM + 4.4 * KM * peaks ** 2.2)
    return h * (0.12 + 0.88 * gap_factor(theta)) - 0.02 * KM


def hills(r, theta):
    x, y = r * math.cos(theta) / (0.9 * KM), r * math.sin(theta) / (0.9 * KM)
    rise = smoothstep(0.55 * KM, 1.3 * KM, r)
    shape = min(1.0, max(0.0, fbm(x, y, 5) * 0.5 + 0.5))
    h = rise * (0.02 * KM + 0.2 * KM * shape ** 1.7 + 0.12 * KM * smoothstep(2.4 * KM, 4.0 * KM, r))
    return h * gap_factor(theta, half_width_deg=30.0) - 0.004 * KM


if __name__ == "__main__":
    ring("SM_DB_Backdrop_Mountains", 3.5 * KM, 14.0 * KM, 1536, 150, mountains,
         lambda z, nz: "Snow" if z > 1.15 * KM + 0.35 * KM * (1.0 - nz) and nz > 0.45 else "Rock")
    ring("SM_DB_Backdrop_Hills", 0.55 * KM, 4.0 * KM, 1024, 110, hills,
         lambda z, nz: "Grass" if nz > 0.82 else "Rock")
