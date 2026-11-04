#!/usr/bin/env python3
# tag-root lane: decode a retail .tan (tag animation) file the way the port's
# BuildTANAnim / TIKI_LoadTAN reads it, and dump every tag pose.
#   python3 dump_tan.py <file.tan>
import struct, sys, math

TAN_TAG_NAME_SIZE = 64
TAN_TAG_POSE_SIZE = 48

def stride(nf):
    return TAN_TAG_NAME_SIZE + nf * TAN_TAG_POSE_SIZE

def name_ofs(ofsTags, nf, t):
    return ofsTags + t * stride(nf)

def pose_ofs(ofsTags, nf, t, f):
    return name_ofs(ofsTags, nf, t) + TAN_TAG_NAME_SIZE + f * TAN_TAG_POSE_SIZE

def main(path):
    b = open(path, 'rb').read()
    print("file=%s size=%d" % (path, len(b)))
    print("ident0x00=%s" % repr(b[0:4]))
    print("version0x04=%d" % struct.unpack_from('<i', b, 4)[0])
    numFrames = struct.unpack_from('<i', b, 0x48)[0]
    numTags   = struct.unpack_from('<i', b, 0x4C)[0]
    totaltime = struct.unpack_from('<f', b, 0x54)[0]
    delta     = struct.unpack_from('<3f', b, 0x58)
    ofsFrames = struct.unpack_from('<i', b, 0x64)[0]
    ofsTags   = struct.unpack_from('<i', b, 0x6C)[0]
    print("numFrames=%d numTags=%d totaltime=%.4f totalDelta=(%.3f,%.3f,%.3f) ofsFrames=0x%X ofsTags=0x%X"
          % (numFrames, numTags, totaltime, delta[0], delta[1], delta[2], ofsFrames, ofsTags))
    # header word census: dump the whole 0x70-byte header as dwords
    print("--- header dwords ---")
    for i in range(0, 0x78, 4):
        v = struct.unpack_from('<i', b, i)[0]
        f = struct.unpack_from('<f', b, i)[0]
        s = b[i:i+4].decode('latin1').replace('\x00', '')
        print("  0x%02X: %-12d %-14.6g %r" % (i, v, f, s))
    print("--- tag block ---")
    for t in range(numTags):
        no = name_ofs(ofsTags, numFrames, t)
        nm = b[no:no+TAN_TAG_NAME_SIZE].split(b'\x00')[0].decode('latin1')
        print("tag[%d] name='%s' @0x%X" % (t, nm, no))
        prev = None
        for f in range(numFrames):
            po = pose_ofs(ofsTags, numFrames, t, f)
            vals = struct.unpack_from('<12f', b, po)
            o = vals[0:3]
            ax = [vals[3:6], vals[6:9], vals[9:12]]
            if prev is None or any(abs(x - y) > 1e-6 for x, y in zip(vals, prev)):
                print("  f%-3d @0x%06X org=(%9.4f,%9.4f,%9.4f)" % (f, po, o[0], o[1], o[2]))
                for r in range(3):
                    print("        ax%d=(%9.5f,%9.5f,%9.5f) len=%.5f" % (r, ax[r][0], ax[r][1], ax[r][2],
                          math.sqrt(sum(c*c for c in ax[r]))))
                prev = vals
            else:
                pass
        if prev is not None:
            print("  (frames after the last printed one are byte-identical to the last print)")

main(sys.argv[1])
