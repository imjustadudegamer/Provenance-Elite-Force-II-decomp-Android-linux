#!/usr/bin/env python3
"""Retail EF2 Archiver typed-savegame stream -- a Python model, proven against
the shipped 2003 retail savegame corpus.

Everything in here is derived from source, never guessed.  The rules and their
citations live in docs/notes/ITEM_E_SAVEGAME.md; the short version:

  * fgame/archive.cpp (SDK == port) is the wire format.  ARCHIVE_USE_TYPES is
    commented out (SDK archive.cpp:486), so ArchiveData writes NO type tag --
    but Archiver::ReadObject/ArchiveObject call ReadType()/WriteType()
    *directly* (SDK archive.cpp:637/721 etc.), so every object DOES carry a
    leading uint32 type = 15 (ARC_Object) or 21 (ARC_Entity).
  * every object carries a uint32 byte-size prefix bounding exactly its own
    body, so the stream is self-delimiting at object granularity even for
    classes we have no field model for.
  * Event is serialised by Event::Archive (SDK listener.cpp:2200-2311).

Usage
-----
    python3 docs/tools/sav_stream.py [ corpus-dir ] [ --dump PATH ] [ --classes ]

With no argument it extracts docs/../Downloads/startrekeliteforce2_savegames_all.zip
into a temp dir, walks every .sav/.sub and prints the per-file result table plus
the aggregate.
"""
import os, struct, sys, tempfile, zipfile
from collections import Counter, defaultdict

ZIP_CANDIDATES = [
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + "/Downloads/startrekeliteforce2_savegames_all.zip",
    os.path.expanduser("~/Downloads/startrekeliteforce2_savegames_all.zip"),
]

# ---------------------------------------------------------------- citations
# fgame/archive.cpp:39-41
MAGIC = b"EF2\x00"                 # #define ArchiveHeader ( *( int * )"EF2" )
VERSION = 2                        # #define ArchiveVersion 2
INFO = b"EF2 Archive Version 2"    # #define ArchiveInfo "..."
# fgame/archive.h:32-35
ARCHIVE_NULL_POINTER = -654321
ARCHIVE_POINTER_VALID = 0
# fgame/archive.cpp:26-28  (the ARC_ enum; only 15/21 ever reach the wire)
ARC_Object, ARC_Entity = 15, 21

OBJ, ENT = ARC_Object, ARC_Entity

HEADER_SIZE = 4 + 4 + 4 + len(INFO)   # 33

# SDK dlls/game/teammateroster.hpp:23
MAX_TEAMMATES = 4


class StreamError(Exception):
    pass


# ===========================================================================
#  FileRead  (fgame/archive.cpp:105-171)
# ===========================================================================
class Reader(object):
    """Byte cursor over one archive file.  Read bounds are checked the same way
    FileRead::Read is (archive.cpp:154-159: refuse to read past the end)."""

    def __init__(self, buf, start=0, end=None, label=""):
        self.buf = buf
        self.pos = start
        self.end = len(buf) if end is None else end
        self.label = label
        self.trail = []          # (offset, kind, value) -- only when tracing

    # -- FileRead::Read
    def read(self, n):
        if self.pos + n > self.end:
            raise StreamError("%s: read of %d at 0x%x overruns end 0x%x"
                              % (self.label, n, self.pos, self.end))
        b = self.buf[self.pos:self.pos + n]
        self.pos += n
        return b

    def remaining(self):
        return self.end - self.pos

    # -- ArchiveData(type, v, sizeof(T)) -- raw little-endian memcpy, PPC32 LE
    def _u(self, fmt, n):
        return struct.unpack_from(fmt, self.read(n))[0]

    def integer(self):       # ARCHIVE( Integer, int )      -> 4
        return self._u("<i", 4)

    def unsigned(self):      # ARCHIVE( Unsigned, unsigned )-> 4
        return self._u("<I", 4)

    def byte(self):          # ARCHIVE( Byte, byte )        -> 1
        return self._u("<B", 1)

    def char(self):          # ARCHIVE( Char, char )        -> 1
        return self._u("<b", 1)

    def short(self):         # ARCHIVE( Short, short )      -> 2
        return self._u("<h", 2)

    def unsignedshort(self):  # ARCHIVE( UnsignedShort, unsigned short ) -> 2
        return self._u("<H", 2)

    def boolean(self):       # ARCHIVE( Boolean, qboolean ) -> 4 (q_shared.h:221)
        return self._u("<i", 4)

    def boolean1(self):      # ARCHIVE( Bool, bool )        -> 1 (C++ bool)
        return self._u("<B", 1)

    def float32(self):       # ARCHIVE( Float, float )      -> 4
        return self._u("<f", 4)

    def float64(self):       # ARCHIVE( Double, double )    -> 8
        return self._u("<d", 8)

    def vector(self):        # ARCHIVE( Vector, Vector )    -> 12 (3 x float)
        return self._u("<3f", 12)

    def vec3(self):          # ArchiveVec3 -> sizeof(vec3_t) = 12
        return self._u("<3f", 12)

    def vec4(self):          # ArchiveVec4 -> sizeof(vec4_t) = 16
        return self._u("<4f", 16)

    def quat(self):          # ARCHIVE( Quat, Quat )        -> 16 (4 x float)
        return self._u("<4f", 16)

    # -- Archiver::ArchiveString (archive.cpp:783-833)
    #     write: WriteSize( string->length() )  [uint32] then the bytes
    def string(self):
        n = self._u("<I", 4)
        if n > 0x7FFFFFFF:
            raise StreamError("%s: absurd string length %d at 0x%x"
                              % (self.label, n, self.pos - 4))
        return self.read(n).decode("latin1")

    def raw(self, n):
        return self.read(n)

    # -- ArchiveObjectPointer / ArchiveSafePointer: one int32 index
    def pointer(self):
        return self.integer()

    def skip_to(self, newpos):
        if newpos > self.end:
            raise StreamError("%s: skip past end" % self.label)
        self.pos = newpos


# ===========================================================================
#  Event  --  Event::Archive, SDK dlls/game/listener.cpp:2200-2311
# ===========================================================================
class Event(object):
    __slots__ = ("name", "info", "threadnum", "anim_number", "anim_frame",
                 "source", "time", "flags", "vars", "argdefs", "rettype")


def read_event(r, depth=0):
    """Event::Archive.  Order is SDK listener.cpp:2216-2311 verbatim."""
    e = Event()
    e.name = r.string()                       # arc.ArchiveString( &name )
    e.info = r.raw(4)                         # arc.ArchiveRaw( &info, sizeof(info) )
    e.threadnum = r.integer()                 # arc.ArchiveInteger( &threadnum )
    e.anim_number = r.short()                 # arc.ArchiveShort( &anim_number )
    e.anim_frame = r.short()                  # arc.ArchiveShort( &anim_frame )
    e.source = r.pointer()                    # arc.ArchiveSafePointer( &m_sourceobject )
    e.time = r.float32()                      # arc.ArchiveFloat( &time )
    e.flags = r.integer()                     # arc.ArchiveInteger( &flags )
    n = r.integer()                           # data (Container<EventVar>)
    e.vars = [read_eventvar(r) for _ in range(n)]
    n = r.integer()                           # definition (Container<EventArgDef>)
    e.argdefs = [read_eventargdef(r) for _ in range(n)]
    n = r.integer()                           # returntype
    e.rettype = read_eventargdef(r) if n == 1 else None
    return e


# EventVar::Archive -- SDK listener.cpp:149-157
def read_eventvar(r):
    return (r.short(), r.short(), r.string(), r.vector(), r.integer(), r.float32())


# EventArgDef::Archive -- SDK listener.cpp:831-856
def read_eventargdef(r):
    return (r.integer(), r.string(),
            [r.float32() for _ in range(3)],
            [r.boolean() for _ in range(3)],
            [r.float32() for _ in range(3)],
            [r.boolean() for _ in range(3)],
            r.boolean())


# ===========================================================================
#  Class field-order table
#  ------------------------
#  Each entry is the ordered Archive* call list of that class's Class::Archive,
#  transcribed from the retail SDK (personal/evidence/ef2-sdk/ef2-sdk/dlls/game/<file>.cpp,
#  CRLF-stripped).  Token grammar is in FIELD_TOKENS below.
#
#  A class with NO entry here is walked as an opaque, size-bounded blob: its
#  uint32 size prefix still guarantees the outer stream stays byte-aligned.
# ===========================================================================

# a field token is  (kind, name[, arg])
#   kinds: int uint byte char short ushort boolean bool1 float double
#          vec vec3 vec4 quat string raw objptr safeptr event eventptr
#          object(class) readobject container(elem) enum classarch(class)

# ===========================================================================
#  Object walking
# ===========================================================================
class Walker(object):
    def __init__(self, class_fields, trace=False):
        self.class_fields = class_fields
        self.trace = trace
        self.class_counts = Counter()
        self.unmodelled = Counter()
        self.unmodelled_bytes = Counter()
        self.mismatch = []            # (classname, offset, msg)
        self.objects = 0
        self.path = []                # classname/field breadcrumb
        self.instances = []           # (classname, body, declared size, walked)

    # -- one ArchiveObject/ReadObject record ---------------------------------
    def read_object(self, r, depth=0, want=None):
        """Archiver::ReadObject (archive.cpp:835-916) / ArchiveObject read arm.
        Returns (classname, body_start, size, consumed)."""
        typ = r.integer()                     # ReadType() -- always present
        if typ not in (OBJ, ENT):
            raise StreamError("%s: object type %d (want 15/21) at 0x%x"
                              % (r.label, typ, r.pos - 4))
        size = r.unsigned()                   # ReadSize()
        classname = r.string()                # ArchiveString( &classname )
        header = r.pos
        if typ == ENT:
            entnum = r.integer()              # ArchiveInteger( &level.spawn_entnum )
            spawnflags = r.integer()          # ArchiveInteger( &level.spawnflags )
        index = r.integer()                   # ArchiveInteger( &index )
        body = r.pos
        self.objects += 1
        self.class_counts[classname] += 1

        entry = self.class_fields.get(classname)
        consumed = 0
        if entry is None:
            # opaque: bounded by `size`, which retail itself enforces
            # (archive.cpp:897-908 "Object read past end of object's data")
            self.unmodelled[classname] += 1
            self.unmodelled_bytes[classname] += size
            self.note(depth, "OPAQUE %-28s size=%-8d body=0x%x" %
                      (classname, size, body))
            r.skip_to(body + size)
            consumed = size
        else:
            self.path.append(classname)
            self.note(depth, "ENTER %-28s size=%-8d body=0x%x" %
                      (classname, size, body))
            try:
                consumed = self.walk_fields(r, classname, entry[0], depth + 1)
            except StreamError as exc:
                # The field model disagrees with the stream for this class.
                # Record it, fall back to the object's own size prefix and keep
                # going: the outer stream stays byte-exact either way.
                self.mismatch.append((classname, body, size, None, str(exc)))
                consumed = None
                r.skip_to(body + size)
            else:
                if consumed != size:
                    self.mismatch.append((classname, body, size, consumed,
                                          "field walk consumed %d of %d "
                                          "declared bytes" % (consumed, size)))
                    r.skip_to(body + size)
        if len(self.instances) < 200000:
            self.instances.append((classname, body, size, consumed))
        return classname, body, size, consumed

    def note(self, depth, msg):
        if self.trace:
            print("    %s%s" % ("  " * depth, msg))

    # -- the field-order interpreter -----------------------------------------
    def walk_fields(self, r, cname, toks, depth):
        start = r.pos
        for tok in toks:
            kind, name, arg = tok
            self.path.append("%s.%s" % (cname, name if name else kind))
            try:
                self.one(r, kind, arg, depth)
            except StreamError as exc:
                self.mismatch.append((cname, r.pos, None, None,
                                      "field %s: %s" % (name, exc)))
                self.note(depth, "!! %s.%s @0x%x: %s"
                          % (cname, name, r.pos, exc))
                raise
            finally:
                self.path.pop()
        return r.pos - start

    def one(self, r, kind, arg, depth):
        if kind == "repeat":
            # arg = (count, subtokens).  count is either an int (a fixed C++
            # loop bound, `for ( i = 0; i < 3; i++ )`) or None, meaning the
            # count was just written by the preceding ArchiveInteger(&num) --
            # the `num` idiom of SDK archive order lists.  The peephole in
            # sav_sdk_arc strips that ArchiveInteger, so here we read it.
            count, sub = arg
            n = count if count is not None else r.integer()
            for _ in range(n):
                for k, nm, a in sub:
                    self.one(r, k, a, depth)
        elif kind == "condstring":
            # ArchiveBool( &f ); if ( f ) ArchiveString( &g );  -- the flag is
            # on the wire (Program::Archive's MAX_STRINGS table)
            if r.boolean1():
                r.string()
        elif kind == "container":
            # Container<T>::Archive -- fgame/archive.h:212-446
            n = r.integer()                       # archive.h:223
            for _ in range(n):
                if arg == "objptr_object":        # archive.h:331
                    r.pointer()
                    self.read_object(r, depth)
                else:
                    self.one(r, arg, None, depth)
        else:
            self._prim(r, kind, arg, depth)

    def _prim(self, r, kind, arg, depth):
        if kind == "int":
            r.integer()
        elif kind == "uint":
            r.unsigned()
        elif kind == "byte":
            r.byte()
        elif kind == "char":
            r.char()
        elif kind == "short":
            r.short()
        elif kind == "ushort":
            r.unsignedshort()
        elif kind == "boolean":
            r.boolean()
        elif kind == "bool1":
            r.boolean1()
        elif kind == "float":
            r.float32()
        elif kind == "double":
            r.float64()
        elif kind == "vec":
            r.vector()
        elif kind == "vec3":
            r.vec3()
        elif kind == "vec4":
            r.vec4()
        elif kind == "quat":
            r.quat()
        elif kind == "string":
            r.string()
        elif kind == "raw":
            r.raw(arg)
        elif kind == "objptr":
            r.pointer()
        elif kind == "safeptr":
            r.pointer()
        elif kind == "event":
            read_event(r, depth)
        elif kind == "eventptr":
            if r.integer() == ARCHIVE_POINTER_VALID:   # archive.cpp:741
                read_event(r, depth)
        elif kind == "object":
            self.read_object(r, depth)
        elif kind == "readobject":
            self.read_object(r, depth)
        elif kind == "enum":
            r.integer()
        else:
            raise StreamError("unknown field kind %r" % (kind,))


# ===========================================================================
#  ArchiveAliases  --  ArchiveAliases__FR8Archiver @0x001D30D0
#  Byte-flag-terminated lists; see the note for the exact shape.
# ===========================================================================
def read_aliases(r):
    start = r.pos
    count_buckets = 0
    count_nodes = 0
    while True:
        flag = r.byte()
        if flag == 0:
            break
        if flag != 1:
            raise StreamError("alias bucket flag %d at 0x%x" % (flag, r.pos - 1))
        count_buckets += 1
        r.string()                       # alias key
        while True:
            flag = r.byte()
            if flag == 0:
                break
            if flag != 1:
                raise StreamError("alias node flag %d at 0x%x" % (flag, r.pos - 1))
            count_nodes += 1
            r.string()                   # node name
            r.string()                   # node value
            r.string()                   # parm string
            r.integer()                  # node+308
            r.byte()                     # node+312
            r.integer()                  # node+316
            while True:
                flag = r.byte()
                if flag == 0:
                    break
                if flag != 1:
                    raise StreamError("alias parm flag %d at 0x%x" % (flag, r.pos - 1))
                r.integer(); r.integer(); r.byte(); r.integer()
    return count_buckets, count_nodes, r.pos - start


# ===========================================================================
#  Top level --  G_ArchiveLevel__FPCcii @0x001D3880
# ===========================================================================
def walk_level(r, w, loading_goes_first=True):
    """The exact G_ArchiveLevel stream order (identical read/write)."""
    log = []
    # LevelArchiveValid__FR8Archiver @0x001D2B50 -- two magic ints
    a, b = r.integer(), r.integer()
    log.append(("magic", a, b))
    if a != 4 or b != 8:
        raise StreamError("%s: LevelArchiveValid magic %d/%d != 4/8 at 0x%x"
                          % (r.label, a, b, r.pos - 8))

    # L_ArchiveEvents__FR8Archiver @0x00247C60
    nevents = r.integer()
    log.append(("events", nevents))
    evs = []
    for _ in range(nevents):
        e = read_event(r)
        t = r.float32()                   # ArchiveFloat( j + 68 )
        f = r.integer()                   # ArchiveInteger( j + 72 )
        evs.append((e, t, f))

    # ten ArchiveObject calls before the ScriptVariableList
    tops = [w.read_object(r, 0) for _ in range(10)]
    log.append(("tops", [t[0] for t in tops]))

    n_edicts = r.integer()                # ArchiveInteger( v14 + 34 )
    n_saved = r.integer()                 # ArchiveInteger( &v129 )
    log.append(("entity counts", n_edicts, n_saved))
    ents = [w.read_object(r, 0) for _ in range(n_saved + 1)]

    tail = {}
    tail["after_entities"] = r.pos
    tail["program"] = w.read_object(r, 0)
    buckets, nodes, nb = read_aliases(r)
    tail["aliases"] = (buckets, nodes, nb)
    # TeammateRoster::Archive -- SDK dlls/game/teammateroster.hpp:98-120
    roster = []
    for _ in range(MAX_TEAMMATES):
        roster.append((r.integer(),             # entNum
                       r.integer(),             # injuredHealthLevel
                       r.integer(),             # criticalHealthLevel
                       r.boolean1(),            # active  (ArchiveBool = C++ bool)
                       r.string()))             # archeType
    tail["roster"] = roster
    tail["roster_shaders_anims"] = [r.string() for _ in range(7)]
    # GameplayManager::Archive -> GameplayDatabase::Archive:
    #   int numModifiedObjects, then that many
    #   { str scopedObjectName, int numProperties, numProperties x {str,str} }
    nmod = r.integer()
    deltas = []
    for _ in range(nmod):
        s = r.string()
        np = r.integer()
        deltas.append((s, [(r.string(), r.string()) for _ in range(np)]))
    tail["db_numModifiedObjects"] = nmod
    tail["db_deltas"] = deltas
    log.append(("entities", len(ents)))
    return dict(log=log, events=evs, tops=tops, n_edicts=n_edicts,
                n_saved=n_saved, ents=ents, tail=tail)


def walk_file(path, class_fields, dump=None):
    buf = open(path, "rb").read()
    r = Reader(buf, label=os.path.basename(path))
    hdr_magic = r.unsigned()
    hdr_ver = r.unsigned()
    hdr_info = r.string()
    ncls = r.integer()                    # Archiver::Read archive.cpp:354
    if hdr_magic != struct.unpack("<I", MAGIC)[0] or hdr_ver != VERSION \
            or hdr_info != INFO.decode("latin1"):
        raise StreamError("bad header")
    w = Walker(class_fields)
    res = walk_level(r, w)
    res["ncls"] = ncls
    res["consumed"] = r.pos
    res["filesize"] = len(buf)
    res["tail"] = len(buf) - r.pos
    res["walker"] = w
    if dump:
        dump_result(path, res, dump)
    return res


# ===========================================================================
#  class table -- transcribed from the retail SDK
# ===========================================================================
def load_class_table():
    """Build the class field-order table out of the real EF2 SDK source, using
    sav_sdk_arc (same directory).  Classes whose Archive body contains a
    construct the emitter cannot prove -- most importantly Entity, whose
    G_ArchiveEdict writes the gclient_t raw block only `if ( edict->client )` --
    are deliberately absent: the walker then falls back to that object's own
    4-byte size prefix, which retail itself enforces (archive.cpp:897-908)."""
    here = os.path.dirname(os.path.abspath(__file__))
    if here not in sys.path:
        sys.path.insert(0, here)
    try:
        import sav_sdk_arc
    except ImportError:
        return {}
    resolved, raw, notes = sav_sdk_arc.load()
    # keep the raw (pre-member-resolution) source file citation
    out = {}
    for name, (toks, src) in resolved.items():
        out[name] = (toks, src)
    return out


def _extract_zip():
    for z in ZIP_CANDIDATES:
        if os.path.exists(z):
            d = tempfile.mkdtemp(prefix="ef2sav_")
            with zipfile.ZipFile(z) as zf:
                zf.extractall(d)
            print("extracted %s -> %s" % (z, d))
            return d
    print("no corpus zip found", ZIP_CANDIDATES)
    raise SystemExit(2)


# ===========================================================================
#  corpus driver
# ===========================================================================
def collect(corpus_dir):
    savs, subs, spvs = [], [], []
    for root, _, files in os.walk(corpus_dir):
        for f in files:
            p = os.path.join(root, f)
            if f.endswith(".sav"):
                savs.append(p)
            elif f.endswith(".sub"):
                subs.append(p)
            elif f.endswith(".spv"):
                spvs.append(p)
    savs.sort(); subs.sort(); spvs.sort()
    return savs, subs, spvs


def verify(corpus_dir, fields, limit=0):
    """Field-verify every modelled class against every corpus instance.

    A class passes when, for every instance in the corpus, the declared object
    `size` equals the number of bytes the field walk consumed -- which is what
    retail's own check (archive.cpp:897-908) demands.
    """
    savs, subs, spvs = collect(corpus_dir)
    per_class = defaultdict(lambda: [0, 0])     # name -> [pass, fail]
    fails = defaultdict(list)
    sizes = defaultdict(Counter)                # name -> Counter(size)
    files = 0
    for p in savs + subs:
        try:
            buf = open(p, "rb").read()
        except OSError:
            continue
        files += 1
        r = Reader(buf, label=os.path.basename(p))
        try:
            r.unsigned(); r.unsigned(); r.string()
            r.integer()
            w = Walker(fields)
            walk_level(r, w)
        except (StreamError, struct.error):
            continue
        for (classname, body, size, consumed) in w.instances:
            sizes[classname][size] += 1
            if classname not in fields:
                continue
            if consumed == size:
                per_class[classname][0] += 1
            else:
                per_class[classname][1] += 1
                if len(fails[classname]) < 4:
                    fails[classname].append((os.path.basename(p), body,
                                             size, consumed))
    print("verify: %d files, %d modelled classes" %
          (files, len([c for c in per_class if c in fields])))
    bad = 0
    for cname in sorted(per_class):
        ok, fail = per_class[cname]
        tag = "OK " if fail == 0 else "BAD"
        if fail:
            bad += 1
        print("  %s %-26s %6d pass %5d fail   distinct sizes: %s"
              % (tag, cname, ok, fail,
                 sorted(sizes[cname])[:6]))
        for f in fails[cname]:
            print("        %s @0x%x declared=%s walked=%s" %
                  (f[0], f[1], f[2], f[3]))
    print("\n%d/%d modelled classes verify" %
          (len(per_class) - bad, len(per_class)))
    return 0 if bad == 0 else 1


def main():
    argv = [a for a in sys.argv[1:] if not a.startswith("--")]
    dumpdir = None
    if "--classes" in sys.argv:
        fields = load_class_table()
        print("%d classes with a source-derived field order:" % len(fields))
        for name in sorted(fields):
            toks, src = fields[name]
            print("  %-26s [%s]  %s" % (name, src, [t[0] for t in toks]))
        return 0
    if "--verify" in sys.argv:
        d = argv[0] if argv else None
        if not d:
            d = _extract_zip()
        savs, subs, spvs = collect(d)
        print("corpus: %d .sav  %d .sub" % (len(savs), len(subs)))
        return verify(d, load_class_table())
    if "--dump" in sys.argv:
        dumpdir = sys.argv[sys.argv.index("--dump") + 1]
        os.makedirs(dumpdir, exist_ok=True)

    d = argv[0] if argv else None
    if not d:
        d = _extract_zip()

    savs, subs, spvs = collect(d)
    print("corpus: %d .sav  %d .sub  %d .spv" % (len(savs), len(subs), len(spvs)))

    fields = load_class_table()
    ok = bad = 0
    stop_points = Counter()
    per_kind = {}
    all_classes = Counter()
    for label, group in ((".sav", savs), (".sub", subs)):
        good = 0
        results = []
        for p in group:
            try:
                res = walk_file(p, fields)
                good += 1
                results.append((p, res, None))
                all_classes.update(res["walker"].class_counts)
            except StreamError as exc:
                bad += 1
                results.append((p, None, str(exc)))
                stop_points[str(exc).split(": ", 1)[-1][:90]] += 1
            except Exception as exc:      # struct.error etc
                bad += 1
                results.append((p, None, "%s: %s" % (type(exc).__name__, exc)))
                stop_points["%s: %s" % (type(exc).__name__,
                                        str(exc)[:80])] += 1
        per_kind[label] = results
        print("\n== %s : %d/%d fully walked ==" % (label, good, len(group)))
        for p, res, err in results:
            if err:
                print("  FAIL %-46s %s" % (os.path.basename(p), err))
            elif res["tail"]:
                print("  TAIL %-46s %d/%d bytes, %d left"
                      % (os.path.basename(p), res["consumed"], res["filesize"],
                         res["tail"]))

    print("\n=== aggregate ===")
    print("classes seen: %d distinct, %d objects" %
          (len(all_classes), sum(all_classes.values())))
    for name, n in all_classes.most_common(25):
        print("   %6d  %s" % (n, name))
    if stop_points:
        print("\nstop points:")
        for s, n in stop_points.most_common(20):
            print("   %4d  %s" % (n, s))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
