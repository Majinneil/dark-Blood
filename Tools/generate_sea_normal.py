# Plain Python 3 (no dependencies):  python Tools/generate_sea_normal.py
#
# Writes SourceArt/Generated/T_DB_Sea_N.png, a seamless normal map of wind-driven sea chop for M_DB_Sea: a sum of
# sine waves with whole-number wave vectors (so the texture tiles) and an amplitude falling with frequency, mostly
# travelling along the wind. OpenGL convention (green up), imported without flipping.
import math
import os
import random
import struct
import zlib

SIZE = 512
WAVES = 40
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "SourceArt", "Generated", "T_DB_Sea_N.png")


def build_waves():
    rng = random.Random(1337)
    waves = []
    wind = math.radians(30.0)
    while len(waves) < WAVES:
        frequency = rng.uniform(2.0, 26.0)
        angle = wind + rng.gauss(0.0, 0.6)
        kx = round(frequency * math.cos(angle))
        ky = round(frequency * math.sin(angle))
        if kx == 0 and ky == 0:
            continue
        length = math.hypot(kx, ky)
        amplitude = 1.0 / length ** 1.6
        waves.append((kx, ky, amplitude, rng.uniform(0.0, 2.0 * math.pi)))
    return waves


def slopes(waves):
    # d(height)/dx and d(height)/dy per pixel; the phase grid is precomputed per wave row/column.
    dx = [[0.0] * SIZE for _ in range(SIZE)]
    dy = [[0.0] * SIZE for _ in range(SIZE)]
    step = 2.0 * math.pi / SIZE
    for kx, ky, amplitude, phase in waves:
        # Sharper crests: cos^3-like shaping through a second harmonic keeps the chop from looking like pure sines.
        for y in range(SIZE):
            row_x = dx[y]
            row_y = dy[y]
            base = phase + ky * y * step
            for x in range(SIZE):
                t = base + kx * x * step
                c = math.cos(t) + 0.35 * math.cos(2.0 * t)
                row_x[x] += amplitude * kx * c
                row_y[x] += amplitude * ky * c
    return dx, dy


def write_png(path, pixels):
    raw = b"".join(b"\x00" + bytes(row) for row in pixels)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as file:
        file.write(b"\x89PNG\r\n\x1a\n")
        file.write(chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2, 0, 0, 0)))
        file.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        file.write(chunk(b"IEND", b""))


def main():
    dx, dy = slopes(build_waves())
    peak = max(max(abs(v) for v in row) for row in dx + dy)
    strength = 1.4 / peak
    pixels = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            nx, ny, nz = -dx[y][x] * strength, -dy[y][x] * strength, 1.0
            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            row += [int(round((v / length * 0.5 + 0.5) * 255)) for v in (nx, ny, nz)]
        pixels.append(row)
    write_png(OUT, pixels)
    print("wrote", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
