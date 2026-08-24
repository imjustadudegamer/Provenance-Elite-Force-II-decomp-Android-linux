import struct, glob, os, collections

files = sorted(glob.glob("retail/Elite Force II/base/save/*/**/*.ssv", recursive=True)) + \
        sorted(glob.glob("retail/Elite Force II/base/save/*/*.ssv"))
files = sorted(set(files))
print("retail .ssv files:", len(files))

rows = []
for p in files:
    d = open(p,'rb').read()
    t, mapTime, f8 = struct.unpack_from("<3i", d, 0)
    comment = d[0x0C:0x4C].split(b'\0')[0].decode('latin1')
    mapName = d[0x4C:0x8C].split(b'\0')[0].decode('latin1')
    saveName= d[0x8C:0xCC].split(b'\0')[0].decode('latin1')
    cc, d0 = struct.unpack_from("<2i", d, 0xCC)
    rows.append((os.path.basename(p), len(d), t, mapTime, f8, cc, d0, comment, mapName, saveName))

print("\n%-26s %6s %11s %9s %9s %6s %6s" % ("file","size","time","mapTime","f@0x08","@0xCC","@0xD0"))
for r in rows[:14]:
    print("%-26s %6d %11d %9d %9d %6d %6d  | %s | %s" % (r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[8],r[7][:28]))

print("\n--- size vs @0xCC correlation ---")
by = collections.defaultdict(list)
for r in rows: by[r[5]].append(r[1])
for k in sorted(by):
    s=set(by[k]); print("  @0xCC=%-4d n=%-3d sizes=%s" % (k, len(by[k]), sorted(s)[:6]))

print("\n--- @0x08 value distribution ---")
v = collections.Counter(r[4] for r in rows)
print("  distinct:", len(v), " sample:", list(v.items())[:8])
print("  min/max:", min(r[4] for r in rows), max(r[4] for r in rows))

print("\n--- @0xD0 distribution ---", collections.Counter(r[6] for r in rows).most_common(6))
