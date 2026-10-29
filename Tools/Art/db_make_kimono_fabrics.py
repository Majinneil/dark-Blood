"""Fabrics of Akaza's kimono (docs/AKAZA_OUTFIT.md), generated procedurally - no third-party textures, no licenses.

    python -I Tools/Art/db_make_kimono_fabrics.py [out folder] [size]

Writes tileable texture sets (BaseColor, Normal, Roughness PNG) into SourceArt/Generated/AkazaOutfit:
- Fabric_IvoryPattern: cream linen, black motifs (asanoha hemp leaves, small crests) - the upper robe
- Fabric_BlackMotif: black silk with sparse, faded cream crests - the lower robe and the sleeves
- Fabric_BlackBorder: the hem band - cream cloud scrolls between double rules (tiles horizontally)
- Fabric_BlackPlain: black cotton with a visible weave - under-kimono, hakama, hood scarf
- Leather_Brown: worn brown leather - belts, straps, sandals
- Rope_Hemp: twisted hemp cord - wraps and the tassel
Patterns repeat seamlessly across the tile; the weave is a twill whose normal map gives the cloth its grain.
"""
import os
import sys

import numpy as np

PROJECT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(PROJECT, "SourceArt", "Generated", "AkazaOutfit")
SIZE = int(sys.argv[2]) if len(sys.argv) > 2 else 2048


def save_png(path, data):
    """data: HxW (gray) or HxWx3 in 0..1."""
    import zlib
    import struct
    data = np.clip(data, 0.0, 1.0)
    if data.ndim == 2:
        data = np.stack([data] * 3, axis=-1)
    pixels = (data * 255.0 + 0.5).astype(np.uint8)
    height, width, _ = pixels.shape
    raw = b"".join(b"\x00" + pixels[row].tobytes() for row in range(height))

    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)

    with open(path, "wb") as handle:
        handle.write(b"\x89PNG\r\n\x1a\n")
        handle.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        handle.write(chunk(b"IDAT", zlib.compress(raw, 6)))
        handle.write(chunk(b"IEND", b""))


# ---- Building blocks (all periodic over the unit tile) ---------------------------------------------------------------

Y, X = np.meshgrid(np.linspace(0.0, 1.0, SIZE, endpoint=False), np.linspace(0.0, 1.0, SIZE, endpoint=False), indexing="ij")
PIXEL = 1.0 / SIZE


def periodic_noise(frequency, seed, octaves=4):
    """Smooth tileable noise from random Fourier components, 0..1."""
    rng = np.random.default_rng(seed)
    total = np.zeros_like(X)
    amplitude = 1.0
    norm = 0.0
    for octave in range(octaves):
        f = frequency * (2 ** octave)
        for _ in range(6):
            kx, ky = rng.integers(-f, f + 1, size=2)
            phase = rng.uniform(0, 2 * np.pi)
            total += amplitude * np.cos(2 * np.pi * (kx * X + ky * Y) + phase)
        norm += amplitude * 6
        amplitude *= 0.5
    return 0.5 + 0.5 * total / norm * 2.2


def line_mask(distance, width):
    """Anti-aliased stroke of a distance field (distance in tile units)."""
    return np.clip((width - np.abs(distance)) / PIXEL + 0.5, 0.0, 1.0)


def twill(threads, seed):
    """Twill weave height (0..1): diagonal ribs and thread-by-thread variation."""
    u = X * threads
    v = Y * threads
    warp = 0.5 + 0.5 * np.cos(2 * np.pi * u)
    weft = 0.5 + 0.5 * np.cos(2 * np.pi * v)
    diagonal = 0.5 + 0.5 * np.cos(2 * np.pi * (u + v) / 3.0)
    slub = periodic_noise(max(threads // 16, 4), seed, 3)
    return np.clip(0.35 * warp + 0.35 * weft + 0.3 * diagonal, 0, 1) * (0.85 + 0.3 * slub)


def normal_from_height(height, strength):
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * strength
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * strength
    normal = np.stack([-dx, -dy, np.ones_like(height)], axis=-1)
    normal /= np.linalg.norm(normal, axis=-1, keepdims=True)
    return normal * 0.5 + 0.5


def asanoha(cells):
    """Hemp-leaf pattern: six-pointed stars of triangles - the classic kimono motif. Distance to its lines."""
    # Triangular lattice: three line families at 0, 60, 120 degrees plus the star spokes (hexagon centres).
    u = X * cells
    v = Y * cells * 2.0 / np.sqrt(3.0) * np.sqrt(3.0) / 2.0 * 2.0
    best = np.full_like(X, 10.0)
    for angle in (0.0, np.pi / 3.0, 2.0 * np.pi / 3.0, np.pi / 6.0, np.pi / 2.0, 5.0 * np.pi / 6.0):
        direction_u, direction_v = np.cos(angle), np.sin(angle)
        spacing = 1.0 if angle in (0.0, np.pi / 3.0, 2.0 * np.pi / 3.0) else np.sqrt(3.0)
        projected = (u * -direction_v + v * direction_u) / spacing
        distance = np.abs(projected - np.round(projected)) * spacing / cells
        best = np.minimum(best, distance)
    return best


def seigaiha(cells):
    """Overlapping wave arcs (blue-sea waves) - distance to the visible arcs."""
    u = X * cells
    v = Y * cells
    best = np.full_like(X, 10.0)
    for row_shift in (0.0, 0.5):
        cu = np.floor(u - row_shift) + 0.5 + row_shift
        cv = np.floor(v * 2.0) / 2.0 + 0.5
        radius = np.hypot(u - cu, (v - cv) * 2.0)
        for ring in (0.18, 0.3, 0.42):
            best = np.minimum(best, np.abs(radius - ring) / cells)
    return best


def scrolls(cells, seed):
    """Tendrils of spirals (karakusa-like): a spiral per cell, its arms joining the neighbours. Distance-like field."""
    rng = np.random.default_rng(seed)
    u = X * cells
    v = Y * cells
    cu = np.floor(u) + 0.5
    cv = np.floor(v) + 0.5
    dx = u - cu
    dy = v - cv
    radius = np.hypot(dx, dy)
    angle = np.arctan2(dy, dx)
    flip = rng.choice([-1.0, 1.0], size=(cells, cells))
    cell_i = np.mod(np.floor(u).astype(int), cells)
    cell_j = np.mod(np.floor(v).astype(int), cells)
    direction = flip[cell_j, cell_i]
    # Archimedean spiral r = a * theta: distance to the nearest turn.
    turn = 0.11
    theta = (angle * direction) % (2 * np.pi)
    k = np.round((radius - turn * theta / (2 * np.pi)) / turn)
    spiral = np.abs(radius - turn * (theta / (2 * np.pi) + k))
    spiral = np.where(radius < 0.42, spiral, 10.0)
    # Leaves: little almond shapes between the spirals.
    leaf_u = (u + 0.5) % 1.0 - 0.5
    leaf_v = (v + 0.5) % 1.0 - 0.5
    leaf = np.abs(np.hypot(leaf_u * 1.0, leaf_v * 2.4) - 0.12)
    return np.minimum(spiral, leaf) / cells


def crests(cells, seed):
    """Small scattered round crests (mon): a ring with a four-petal flower inside. Filled mask 0..1."""
    rng = np.random.default_rng(seed)
    u = X * cells
    v = Y * cells
    offsets = rng.uniform(-0.18, 0.18, size=(cells, cells, 2))
    cell_i = np.mod(np.floor(u).astype(int), cells)
    cell_j = np.mod(np.floor(v).astype(int), cells)
    ox = offsets[cell_j, cell_i, 0]
    oy = offsets[cell_j, cell_i, 1]
    dx = (u % 1.0) - 0.5 - ox
    dy = (v % 1.0) - 0.5 - oy
    radius = np.hypot(dx, dy)
    angle = np.arctan2(dy, dx)
    ring = np.clip((0.012 - np.abs(radius - 0.16)) * cells / PIXEL + 0.5, 0, 1)
    petals = np.clip(((0.1 + 0.04 * np.cos(4 * angle)) - radius) * cells / PIXEL + 0.5, 0, 1)
    keep = rng.random((cells, cells)) < 0.55
    return np.maximum(ring, petals) * keep[cell_j, cell_i]


def grime(seed):
    """Wear: darker, dusty patches - a worn travelling coat, not a new one."""
    return periodic_noise(3, seed, 5)


# ---- Fabrics ---------------------------------------------------------------------------------------------------------

def write_set(name, base, height, roughness, normal_strength):
    folder = OUT
    os.makedirs(folder, exist_ok=True)
    save_png(os.path.join(folder, "T_%s_BC.png" % name), base)
    save_png(os.path.join(folder, "T_%s_N.png" % name), normal_from_height(height, normal_strength))
    save_png(os.path.join(folder, "T_%s_R.png" % name), roughness)
    print("fabric %s written (%d px)" % (name, SIZE))


def warped_noise(frequency, seed, warp=0.35):
    """Domain-warped tileable noise: flowing, cloud-like shapes."""
    rng = np.random.default_rng(seed)
    total = np.zeros_like(X)
    wx = (periodic_noise(frequency, seed + 101, 3) - 0.5) * warp
    wy = (periodic_noise(frequency, seed + 202, 3) - 0.5) * warp
    for octave in range(4):
        f = frequency * (2 ** octave)
        for _ in range(5):
            kx, ky = rng.integers(-f, f + 1, size=2)
            phase = rng.uniform(0, 2 * np.pi)
            total += 0.5 ** octave * np.cos(2 * np.pi * (kx * (X + wx) + ky * (Y + wy)) + phase)
    return total


def contour_lines(field, spacing, width):
    """Evenly thick lines along the iso-contours of a field (organic linework)."""
    gy, gx = np.gradient(field, PIXEL)
    gradient = np.hypot(gx, gy) + 1e-6
    phase = field / spacing
    distance = np.abs(phase - np.round(phase)) * spacing / gradient
    return line_mask(distance, width)


def stamps(cells, seed, motifs, density):
    """Hand-printed scatter: one motif per jittered cell, random kind, rotation and size. Filled mask 0..1."""
    rng = np.random.default_rng(seed)
    centers = rng.uniform(0.2, 0.8, size=(cells, cells, 2))
    kinds = rng.integers(0, len(motifs), size=(cells, cells))
    rotations = rng.uniform(0, 2 * np.pi, size=(cells, cells))
    scales = rng.uniform(0.75, 1.25, size=(cells, cells))
    keep = rng.random((cells, cells)) < density
    u = X * cells
    v = Y * cells
    result = np.zeros_like(X)
    base_i = np.floor(u).astype(int)
    base_j = np.floor(v).astype(int)
    for di in (-1, 0, 1):
        for dj in (-1, 0, 1):
            ci = np.mod(base_i + di, cells)
            cj = np.mod(base_j + dj, cells)
            dx = u - (base_i + di + centers[cj, ci, 0])
            dy = v - (base_j + dj + centers[cj, ci, 1])
            rot = rotations[cj, ci]
            scale = scales[cj, ci]
            lx = (dx * np.cos(rot) + dy * np.sin(rot)) / scale
            ly = (-dx * np.sin(rot) + dy * np.cos(rot)) / scale
            radius = np.hypot(lx, ly)
            angle = np.arctan2(ly, lx)
            for index, motif in enumerate(motifs):
                mask = (kinds[cj, ci] == index) & keep[cj, ci] & (radius < 0.5)
                if not mask.any():
                    continue
                result = np.maximum(result, np.where(mask, motif(lx, ly, radius, angle, cells * scale), 0.0))
    return result


def fill(sdf, cells):
    return np.clip(-sdf * cells / PIXEL + 0.5, 0.0, 1.0)


def stroke(sdf, width, cells):
    return np.clip((width - np.abs(sdf)) * cells / PIXEL + 0.5, 0.0, 1.0)


def sakura(lx, ly, radius, angle, cells):
    """Five notched petals and a hollow heart."""
    petal = 0.13 * (0.55 + 0.45 * np.abs(np.cos(2.5 * angle))) - 0.025 * np.clip(np.cos(5 * angle) * 8 - 7, 0, 1)
    flower = fill(radius - petal, cells)
    heart = fill(radius - 0.025, cells)
    return np.clip(flower - heart, 0, 1)


def kiku(lx, ly, radius, angle, cells):
    """Chrysanthemum: sixteen thin petals in an outline."""
    edge = 0.15 + 0.03 * np.cos(16 * angle)
    return np.maximum(stroke(radius - edge, 0.008, cells), fill(radius - 0.03, cells))


def tomoe(lx, ly, radius, angle, cells):
    """Three comma swirls in a ring - the mitsudomoe crest."""
    swirl = np.zeros_like(radius)
    for k in range(3):
        a = angle + k * 2 * np.pi / 3
        head = np.hypot(lx - 0.07 * np.cos(-k * 2 * np.pi / 3), ly - 0.07 * np.sin(-k * 2 * np.pi / 3))
        tail = np.abs(radius - (0.07 + 0.05 * ((a % (2 * np.pi)) / (2 * np.pi)))) - 0.012
        swirl = np.maximum(swirl, np.maximum(fill(head - 0.045, cells), fill(tail, cells) * (radius > 0.05)))
    return np.maximum(swirl, stroke(radius - 0.16, 0.007, cells))


def leaf(lx, ly, radius, angle, cells):
    """An almond leaf with a midrib."""
    body = fill(np.hypot(lx * 1.0, ly * 2.6) - 0.15, cells)
    rib = stroke(ly, 0.004, cells) * (np.abs(lx) < 0.13)
    return np.clip(body - rib, 0, 1)


def print_wear(seed):
    """Block-print imperfection: the ink thins in places and the edges break up."""
    return np.clip(periodic_noise(24, seed, 4) * 1.6 - 0.25, 0.0, 1.0)


def ivory_pattern():
    weave = twill(220, 11)
    cream = np.array([0.80, 0.75, 0.65])
    ink = np.array([0.08, 0.06, 0.05])
    # Dense, block-printed like the reference: big crests and leaves, a field of small ones, and between them
    # patches of scrolling vines (karakusa) - so the cloth reads as richly patterned from a distance.
    large = stamps(9, 12, [sakura, kiku, tomoe, leaf], 0.95)
    small = stamps(22, 14, [sakura, leaf, kiku], 0.85)
    tiny = stamps(46, 15, [sakura, leaf], 0.55)
    patches = np.clip((warped_noise(3, 17) - 0.42) * 5.0, 0.0, 1.0)
    vines = line_mask(scrolls(11, 19), 0.0016) * patches
    # Printed rules: a double line with a chain of tiny crests between, twice per tile.
    rule_v = (Y * 2.0) % 1.0
    rules = np.maximum(line_mask(rule_v - 0.46, 0.0011), line_mask(rule_v - 0.54, 0.0011))
    chain = stamps(36, 18, [tomoe], 1.0) * (np.abs(rule_v - 0.5) < 0.03)
    clear = np.abs(rule_v - 0.5) > 0.06
    field = np.maximum(np.maximum(large, small * 0.9), np.maximum(tiny * 0.8, vines * 0.85)) * clear
    motif = np.maximum(field, np.maximum(rules, chain))
    motif = np.clip(motif * (0.5 + 0.5 * print_wear(16)), 0, 1)
    wear = grime(13)
    # Old dye: the cream is warmer and darker in washes, as in a coat that has travelled for years.
    wash = np.clip((warped_noise(2, 21) - 0.45) * 1.2, 0.0, 1.0)
    cream_map = cream[None, None, :] * (1.0 - 0.07 * wash[..., None]) + np.array([0.03, 0.015, 0.0]) * wash[..., None]
    color = cream_map * (0.985 + 0.02 * weave[..., None]) * (0.93 + 0.1 * wear[..., None])
    color = color * (1 - motif[..., None]) + ink[None, None, :] * motif[..., None]
    height = weave * 0.25 + motif * 0.3   # fine weave comes from a tiled detail normal in the engine
    rough = 0.82 + 0.1 * weave - 0.08 * motif
    write_set("Fabric_IvoryPattern", color, height, rough, 3.0)


def thread_color(ornament, weave):
    thread = np.array([0.74, 0.67, 0.55])
    stitch = 0.85 + 0.15 * (0.5 + 0.5 * np.cos(2 * np.pi * (X + Y) * 900))
    return thread[None, None, :] * (ornament * stitch * (0.88 + 0.12 * weave))[..., None]


def black_motif():
    """Black silk with sparse, faded cream crests - the body of the lower robe and the sleeves."""
    weave = twill(260, 21)
    black = np.array([0.022, 0.021, 0.024])
    ornament = stamps(7, 27, [kiku, tomoe, sakura], 0.4)
    ornament = ornament * np.clip(periodic_noise(2, 23, 3) * 1.4 - 0.2, 0.2, 0.9) * (0.6 + 0.4 * print_wear(26))
    wear = grime(24)
    color = black[None, None, :] * (0.8 + 0.4 * weave[..., None]) * (0.92 + 0.25 * wear[..., None])
    color = color * (1 - ornament[..., None]) + thread_color(ornament, weave)
    write_set("Fabric_BlackMotif", color, weave * 0.7 + ornament * 0.6, 0.6 + 0.15 * weave + 0.15 * ornament, 3.5)


def black_border():
    """The hem band: cream cloud scrolls between double rules, black above and below. Tiles horizontally; the band
    lies between V 0.15 and 0.85 of the tile (garment UVs map hems onto it)."""
    weave = twill(260, 31)
    black = np.array([0.022, 0.021, 0.024])
    inside = (Y > 0.2) & (Y < 0.8)
    # Karakusa: a wavy vine along the band; at every crest and trough a spiral tendril curls away from it, with
    # leaves between. Four waves per tile, so it repeats seamlessly to the side.
    waves_per_tile = 4
    amplitude = 0.09
    phase = 2 * np.pi * X * waves_per_tile
    stem_y = 0.5 + amplitude * np.sin(phase)
    slope = amplitude * 2 * np.pi * waves_per_tile * np.cos(phase)
    stem = line_mask((Y - stem_y) / np.sqrt(1 + slope ** 2), 0.0055)
    tendrils = np.zeros_like(X)
    leaves = np.zeros_like(X)
    outer = 0.075
    turn = 0.019
    for n in range(waves_per_tile):
        for crest in (True, False):
            # The spiral rests on the crest (or hangs under the trough): its outermost turn touches the vine there.
            cx = ((0.25 if crest else 0.75) + n) / waves_per_tile
            cy = 0.5 + (amplitude + outer) * (1 if crest else -1)
            dx = (X - cx + 0.5) % 1.0 - 0.5
            dy = Y - cy
            radius = np.hypot(dx, dy)
            # Opening towards the vine; the curl turns inward.
            angle = np.arctan2(dy, dx) * (1.0 if crest else -1.0) + np.pi / 2
            theta = angle % (2 * np.pi)
            ring = np.round((radius - turn * theta / (2 * np.pi)) / turn)
            spiral = np.abs(radius - turn * (theta / (2 * np.pi) + ring))
            tendrils = np.maximum(tendrils, line_mask(np.where(radius < outer, spiral, 1.0), 0.0032))
            # Two leaves on the vine's flanks on either side of the spiral, pointing along the flow.
            for side in (-1.0, 1.0):
                lx0 = cx + side * 0.07 / waves_per_tile * 4
                ly0 = 0.5 + amplitude * np.sin(2 * np.pi * lx0 * waves_per_tile)
                direction = np.arctan(amplitude * 2 * np.pi * waves_per_tile * np.cos(2 * np.pi * lx0 * waves_per_tile))
                tilt = direction + (0.7 if crest else -0.7) * side
                ldx = (X - lx0 + 0.5) % 1.0 - 0.5
                ldy = Y - ly0
                along = ldx * np.cos(tilt) + ldy * np.sin(tilt)
                across = -ldx * np.sin(tilt) + ldy * np.cos(tilt)
                shape = np.hypot((along - side * 0.028) * 1.0, across * 2.8) - 0.026
                body = np.clip(-shape / PIXEL + 0.5, 0, 1)
                rib = line_mask(across, 0.0006) * (np.abs(along - side * 0.028) < 0.022)
                leaves = np.maximum(leaves, np.clip(body - rib, 0, 1))
    curls = stamps(16, 33, [kiku, sakura], 0.75) * 0.85 * (np.abs(Y - 0.5) > 0.24)
    ornament = np.where(inside, np.maximum(np.maximum(stem, tendrils), np.maximum(leaves, curls)), 0.0)
    rules = np.zeros_like(X)
    for edge in (0.15, 0.18, 0.82, 0.85):
        rules = np.maximum(rules, line_mask(Y - edge, 0.003))
    waves = line_mask(seigaiha(24), 0.0006) * ((Y > 0.05) & (Y < 0.13)) * 0.8
    ornament = np.clip(np.maximum(np.maximum(ornament, rules), waves) * (0.65 + 0.35 * print_wear(34)), 0, 1)
    wear = grime(35)
    color = black[None, None, :] * (0.8 + 0.4 * weave[..., None]) * (0.92 + 0.25 * wear[..., None])
    color = color * (1 - ornament[..., None]) + thread_color(ornament, weave)
    write_set("Fabric_BlackBorder", color, weave * 0.7 + ornament * 0.7, 0.6 + 0.15 * weave + 0.15 * ornament, 3.5)


def black_plain():
    weave = twill(180, 31)
    wear = grime(32)
    base = np.array([0.03, 0.03, 0.032])
    color = base[None, None, :] * (0.75 + 0.5 * weave[..., None]) * (0.9 + 0.4 * wear[..., None])
    write_set("Fabric_BlackPlain", color, weave, 0.85 + 0.1 * weave, 3.0)


def leather_brown():
    grain = periodic_noise(40, 41, 4)
    cracks = line_mask(periodic_noise(9, 42, 3) - 0.5, 0.0012) * 0.6
    wear = grime(43)
    brown = np.array([0.24, 0.14, 0.07])
    color = brown[None, None, :] * (0.7 + 0.5 * grain[..., None]) * (0.75 + 0.45 * wear[..., None]) * (1 - 0.4 * cracks[..., None])
    height = grain * 0.6 - cracks * 0.8
    write_set("Leather_Brown", color, height, 0.55 + 0.25 * grain + 0.15 * wear, 4.0)


def rope_hemp():
    twist = 0.5 + 0.5 * np.cos(2 * np.pi * (X * 24 + Y * 48))
    fibre = periodic_noise(64, 51, 3)
    color = np.array([0.42, 0.34, 0.22])[None, None, :] * (0.65 + 0.45 * twist[..., None]) * (0.85 + 0.3 * fibre[..., None])
    write_set("Rope_Hemp", color, twist * 0.8 + fibre * 0.2, 0.9 - 0.1 * twist, 5.0)


def weave_detail(size=512, threads=32):
    """Fine twill weave as a small, densely tiled detail normal (T_Weave_N): the cloth materials repeat it about 40
    times per fabric tile, so the threads are sub-millimetre where the base textures cannot resolve them."""
    y, x = np.meshgrid(np.linspace(0.0, 1.0, size, endpoint=False), np.linspace(0.0, 1.0, size, endpoint=False), indexing="ij")
    u, v = x * threads, y * threads
    over = np.where((np.floor(u) + np.floor(v)) % 3 == 0, 1.0, 0.0)          # 2/1 twill: the float steps diagonally
    warp = np.sin(np.pi * (u % 1.0)) ** 0.7
    weft = np.sin(np.pi * (v % 1.0)) ** 0.7
    height = over * warp + (1.0 - over) * weft
    save_png(os.path.join(OUT, "T_Weave_N.png"), normal_from_height(height, 2.0))
    print("weave detail written (%d px)" % size)


if __name__ == "__main__":
    weave_detail()
    ivory_pattern()
    black_motif()
    black_border()
    black_plain()
    leather_brown()
    rope_hemp()
    print("written to", OUT)
