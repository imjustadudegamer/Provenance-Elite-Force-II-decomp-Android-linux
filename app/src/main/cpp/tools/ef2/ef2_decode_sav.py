#!/usr/bin/env python3
"""
Decode a RETAIL Star Trek: Elite Force II .sav / .sub level archive.

Verified against the whole shipped corpus: all 116 retail .sav files decode to
EXACTLY end-of-file with zero bytes left over.

The format is the EF2 SDK's Archiver (docs/evidence/ef2-sdk/dlls/game/archive.cpp) with
ARCHIVE_USE_TYPES undefined, so plain data carries no type tag -- but objects
always do. Retail is a 32-bit binary: every size/length field is 4 bytes.

Usage:  ef2_decode_sav.py <file.sav> [-v]
        ef2_decode_sav.py --corpus <dir>      # check every .sav below <dir>
"""
import struct, sys, os, glob, collections

ARC_OBJECT = 0x0F
ARC_ENTITY = 0x15


class Reader:
    def __init__(self, path):
        self.d = open(path, 'rb').read()
        self.o = 0

    def i32(self):
        v = struct.unpack_from("<i", self.d, self.o)[0]; self.o += 4; return v

    def i16(self):
        v = struct.unpack_from("<h", self.d, self.o)[0]; self.o += 2; return v

    def f32(self):
        v = struct.unpack_from("<f", self.d, self.o)[0]; self.o += 4; return v

    def vec(self):
        v = struct.unpack_from("<3f", self.d, self.o); self.o += 12; return v

    def byte(self):
        v = self.d[self.o]; self.o += 1; return v

    def raw(self, n):
        v = self.d[self.o:self.o + n]; self.o += n; return v

    def string(self):
        """ArchiveString: 4-byte length then that many raw chars, NO terminator."""
        n = struct.unpack_from("<I", self.d, self.o)[0]; self.o += 4
        v = self.d[self.o:self.o + n].decode('latin1'); self.o += n
        return v


def event_var(r):
    """EventVar::Archive -- short type, short dirtyFlags, str, Vector, int, float."""
    return dict(type=r.i16(), dirty=r.i16(), s=r.string(),
                v=r.vec(), i=r.i32(), f=r.f32())


def event_argdef(r):
    """EventArgDef::Archive -- int type, str name, 3f/3b min, 3f/3b max, bool optional."""
    t = r.i32(); nm = r.string()
    [r.f32() for _ in range(3)]; [r.i32() for _ in range(3)]
    [r.f32() for _ in range(3)]; [r.i32() for _ in range(3)]
    r.i32()
    return dict(type=t, name=nm)


def read_object(r):
    """int tag, int size, str classname, [entity: int spawn_entnum, int spawnflags],
       int index, then exactly `size` body bytes (objstart is taken AFTER the index)."""
    save = r.o
    if r.o + 16 > len(r.d):
        return None
    tag = r.i32()
    if tag not in (ARC_OBJECT, ARC_ENTITY):
        r.o = save; return None
    size = r.i32(); cls = r.string()
    ent = (r.i32(), r.i32()) if tag == ARC_ENTITY else None
    idx = r.i32(); body = r.o
    if body + size > len(r.d):
        r.o = save; return None
    r.o = body + size
    return dict(off=save, tag=tag, size=size, cls=cls, index=idx, ent=ent)


def decode(path, verbose=False):
    r = Reader(path)

    magic = r.raw(4)
    if magic != b'EF2\x00':
        raise ValueError("not an EF2 archive: %r" % magic)
    arcver = r.i32()
    info = r.string()
    numclasses = r.i32()
    api = r.i32()
    savever = r.i32()

    # ---- L_ArchiveEvents -------------------------------------------------
    nev = r.i32()
    events = []
    for _ in range(nev):
        name = r.string(); r.raw(4)                    # EventInfo bitfield
        r.i32(); r.i16(); r.i16()                      # threadnum, anim_number/frame
        src = r.i32(); t = r.f32(); flags = r.i32()
        data = [event_var(r) for _ in range(r.i32())]
        [event_argdef(r) for _ in range(r.i32())]
        if r.i32() == 1:                               # optional returntype object
            read_object(r)
        qtime = r.f32(); qflags = r.i32()              # the queue node's own copy
        events.append(dict(name=name, time=t, flags=flags, src=src,
                           nvar=len(data), qtime=qtime, qflags=qflags))

    # ---- the ten fixed top-level objects ---------------------------------
    tops = [read_object(r) for _ in range(10)]

    # ---- entities ---------------------------------------------------------
    num_entities = r.i32(); declared = r.i32()
    ents = collections.Counter()
    while True:
        o = read_object(r)
        if not o:
            break
        ents[o['cls']] += 1

    # ---- ArchiveAliases: nested "another"-byte-terminated lists -----------
    models = aliases = actors = 0
    another = r.byte()
    while another:
        r.string(); models += 1
        a2 = r.byte()
        while a2:
            r.string(); r.string(); r.string()         # alias, real name, parms
            r.i32(); r.byte(); r.i32()
            aliases += 1
            a3 = r.byte()
            while a3:
                r.i32(); r.i32(); r.byte(); r.i32()
                actors += 1
                a3 = r.byte()
            a2 = r.byte()
        another = r.byte()

    # ---- TeammateRoster::Archive (MAX_TEAMMATES == 4) ---------------------
    mates = []
    for _ in range(4):
        mates.append(dict(entNum=r.i32(), injured=r.i32(), critical=r.i32(),
                          active=r.byte(), archeType=r.string()))
    shaders = [r.string() for _ in range(4)]           # healthy/injured/critical/default
    anims = [r.string() for _ in range(3)]             # healthy/injured/critical

    # ---- GameplayManager::Archive: database deltas ------------------------
    ndelta = r.i32()

    res = dict(path=path, archiveVersion=arcver, info=info, numclasses=numclasses,
               gameApiVersion=api, savegameVersion=savever, events=events,
               tops=[t['cls'] if t else None for t in tops],
               num_entities=num_entities, declared=declared,
               entities=ents, models=models, aliases=aliases, actors=actors,
               mates=mates, shaders=shaders, anims=anims, gameplayDeltas=ndelta,
               consumed=r.o, size=len(r.d))
    if verbose:
        report(res)
    return res


def report(x):
    print("%s" % os.path.basename(x['path']))
    print("  archive v%d %r  classes=%d  GAME_API=%d  SAVEGAME=%d"
          % (x['archiveVersion'], x['info'], x['numclasses'],
             x['gameApiVersion'], x['savegameVersion']))
    print("  events: %d" % len(x['events']))
    for e in x['events'][:6]:
        print("     %-24s t=%.3f flags=%d src=%d nvar=%d"
              % (e['name'], e['time'], e['flags'], e['src'], e['nvar']))
    print("  top-level: %s" % ", ".join(str(c) for c in x['tops']))
    print("  entities: num_entities=%d declared=%d actual=%d across %d classes"
          % (x['num_entities'], x['declared'], sum(x['entities'].values()),
             len(x['entities'])))
    for cls, n in x['entities'].most_common(12):
        print("     %-30s %d" % (cls, n))
    print("  aliases: %d models / %d aliases / %d actor nodes"
          % (x['models'], x['aliases'], x['actors']))
    print("  teammates: %s" % [m['entNum'] for m in x['mates']])
    print("  shaders: %s" % x['shaders'])
    print("  gameplay deltas: %d" % x['gameplayDeltas'])
    print("  consumed 0x%X of 0x%X  (%d bytes left)"
          % (x['consumed'], x['size'], x['size'] - x['consumed']))


def main(argv):
    if len(argv) > 1 and argv[1] == "--corpus":
        root = argv[2] if len(argv) > 2 else "."
        ok = bad = 0
        for p in sorted(glob.glob(os.path.join(root, "**", "*.sav"), recursive=True)):
            try:
                x = decode(p)
                if x['consumed'] == x['size']:
                    ok += 1
                else:
                    bad += 1
                    print("SHORT %-30s %d bytes left"
                          % (os.path.basename(p), x['size'] - x['consumed']))
            except Exception as e:
                bad += 1
                print("FAIL  %-30s %s" % (os.path.basename(p), str(e)[:70]))
        print("%d/%d decode to exactly end-of-file" % (ok, ok + bad))
        return 0 if bad == 0 else 1

    if len(argv) < 2:
        print(__doc__)
        return 2
    decode(argv[1], verbose=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
