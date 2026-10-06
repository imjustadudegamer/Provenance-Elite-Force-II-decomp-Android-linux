#!/usr/bin/env python3
# Independent DXT3 (BC2) decoder.  Written from the DDS/DXT3 spec, not from the
# port's code, so its numbers are independent evidence about the FILE.
#   5-bit -> 8-bit: bit replication (r<<3)|(r>>2)   [exact, standard]
#   6-bit -> 8-bit: bit replication (g<<2)|(g>>4)
#   alpha  4-bit -> 8-bit: a*17
import struct, sys

def exp5(v):
    return ((v << 3) | (v >> 2)) & 255

def exp6(v):
    return ((v << 2) | (v >> 4)) & 255

def decode_dxt3(data, w, h):
    bw, bh = (w + 3) // 4, (h + 3) // 4
    out = [[(0, 0, 0, 0)] * w for _ in range(h)]
    for by in range(bh):
        for bx in range(bw):
            o = (by * bw + bx) * 16
            a = struct.unpack_from('<Q', data, o)[0]        # 16x 4-bit alpha
            c0, c1 = struct.unpack_from('<HH', data, o + 8)
            bits = struct.unpack_from('<I', data, o + 12)[0]
            r0, g0, b0 = exp5(c0 >> 11), exp6((c0 >> 5) & 63), exp5(c0 & 31)
            r1, g1, b1 = exp5(c1 >> 11), exp6((c1 >> 5) & 63), exp5(c1 & 31)
            # DXT3 always uses the 4-colour opaque palette (never the 3-colour
            # transparent variant -- DXT3 alpha carries transparency explicitly)
            pal = [(r0, g0, b0), (r1, g1, b1),
                   ((2 * r0 + r1) // 3, (2 * g0 + g1) // 3, (2 * b0 + b1) // 3),
                   ((r0 + 2 * r1) // 3, (g0 + 2 * g1) // 3, (b0 + 2 * b1) // 3)]
            for py in range(4):
                for px in range(4):
                    x, y = bx * 4 + px, by * 4 + py
                    if x >= w or y >= h:
                        continue
                    idx = py * 4 + px
                    sel = (bits >> (2 * idx)) & 3
                    av = (a >> (4 * idx)) & 15
                    r, g, b = pal[sel]
                    out[y][x] = (r, g, b, av * 17)
    return out

def stats(px, label):
    n = len(px)
    means = [sum(p[i] for p in px) / n for i in range(4)]
    mx = [max(p[i] for p in px) for i in range(4)]
    mn = [min(p[i] for p in px) for i in range(4)]
    print(f"  {label}: n={n} mean=({','.join('%.2f' % m for m in means)}) "
          f"max=({','.join(str(v) for v in mx)}) min=({','.join(str(v) for v in mn)})")
    return means

def main(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'DDS ', 'not dds'
    h, w = struct.unpack_from('<II', d, 12)
    mips = struct.unpack_from('<I', d, 28)[0]
    off = 128
    print(f"{path}: {w}x{h} mips={mips} fourcc={d[84:88]}")
    for m in range(mips):
        mw, mh = max(1, w >> m), max(1, h >> m)
        bw, bh = (mw + 3) // 4, (mh + 3) // 4
        sz = bw * bh * 16
        blk = d[off:off + sz]
        if len(blk) < sz:
            print(f"  mip{m} {mw}x{mh}: TRUNCATED have={len(blk)} need={sz}")
            break
        px = decode_dxt3(blk, mw, mh)
        flat = [p for row in px for p in row]
        stats(flat, f"mip{m} {mw}x{mh}")
        if m == 0:
            cy, cx = mh // 2, mw // 2
            print(f"  mip0 mid texel ({cx},{cy}) = {px[cy][cx]}")
            print(f"  mip0 row {cy} (16 of 32) = {[px[cy][x] for x in range(0, mw, 2)]}")
            print(f"  mip0 col {cx} (16 of 32) = {[px[y][cx] for y in range(0, mh, 2)]}")
        off += sz

main(sys.argv[1])
