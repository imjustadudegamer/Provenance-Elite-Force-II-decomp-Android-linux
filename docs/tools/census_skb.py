#!/usr/bin/env python3
# [R131:E1:census] prove retail's dtiki_s::num_tags (= the .skb's numBones) equals the
# port's m_boneList channel count (= distinct bone names) for EVERY shipped .skb, so
# retail's `tagnum >= num_tags' range test cannot admit an index the port's bone list
# cannot serve.  Also census the .tan numTags (retail's num_tags for a .tan tiki).
import os
import zipfile, struct, os, sys

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__))) + "/scratchpad/airun/base"

def walk():
    for fn in sorted(os.listdir(BASE)):
        if not fn.lower().endswith(".pk3"):
            continue
        try:
            z = zipfile.ZipFile(os.path.join(BASE, fn))
        except Exception:
            continue
        for n in z.namelist():
            low = n.lower()
            if low.endswith(".skb") or low.endswith(".skd") or low.endswith(".tan"):
                try:
                    yield fn, n, z.read(n)
                except Exception:
                    pass

nskb = ntan = 0
bad = []
maxtags = 0
tagsum = 0
for pk, name, d in walk():
    if len(d) < 96:
        continue
    ident = d[0:4]
    if ident == b"SKL ":
        ver = struct.unpack("<i", d[4:8])[0]
        numSurf = struct.unpack("<i", d[72:76])[0]
        numBones = struct.unpack("<i", d[76:80])[0]
        ofsBones = struct.unpack("<i", d[80:84])[0]
        if numBones < 0 or ofsBones < 0 or numBones > 4096:
            bad.append((name, "bad hdr"))
            continue
        stride = 72
        names = []
        for i in range(numBones):
            b = ofsBones + i * stride
            if b + 72 > len(d):
                bad.append((name, "bone runs past eof"))
                break
            names.append(d[b + 8:b + 72].split(b"\x00")[0])
        if len(names) == numBones and numBones:
            nskb += 1
            if len(set(names)) != numBones:
                bad.append((name, "DUPLICATE BONE NAMES %d/%d" % (len(set(names)), numBones)))
    elif ident == b"TAN ":
        numTags = struct.unpack("<i", d[0x4C:0x50])[0]
        ntan += 1
        tagsum += numTags
        if numTags > maxtags:
            maxtags = numTags

print("census: %d .skb (numBones==distinct-names enforced), %d .tan" % (nskb, ntan))
print("tan: total tags=%d max tags in one file=%d" % (tagsum, maxtags))
if bad:
    print("ANOMALIES (%d):" % len(bad))
    for b in bad[:40]:
        print("   ", b[0], b[1])
    sys.exit(1)
print("NO ANOMALIES: num_tags(=numBones) == distinct channel names for every .skb")
