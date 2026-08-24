#!/usr/bin/env python3
"""Recover the string-pool base of a UI translation unit's __sinit__, and decode
its Event / ClassDef registrations.

WHY THIS EXISTS
---------------
Every uilib / cl_ui TU has one `__sinit__:<tu>` that builds its `Event` globals
and its `ClassDef`s.  Each Event ctor is a straight-line block

    lwz  r3, -TOC(r2)          ; the Event global's storage
    addi r4, rBASE, <name>     ; command
    li   r5, <type>            ; event type (-1 = EV_DEFAULT)
    <r6 = argspec>             ; "" / NULL / "s" / "fff" ...
    <r7 = argnames>
    <r8 = desc>
    bl   Event::Event(const char*, int, const char*, const char*, const char*)

and each ClassDef ctor

    addi r4, rBASE, <classname>
    addi r6, rBASE, <supername>
    li   r9, <classsize>
    bl   ClassDef::ClassDef(...)

`rBASE` is a pool pointer loaded from the TOC, so `addi` offsets are only
meaningful once the base is known.  uisweep_0908.md section 5 recovered it by
SCORING: pick the candidate base that makes the largest number of the TU's
offsets land on *some* string.  That reproduces 15 of the 20 TUs whose base is
otherwise known and silently returns a wrong-but-plausible base on the other
five -- every one of them a TU with more than one ClassDef, where several class
names compete for the same high score.  Its 16 guesses for the remaining
translation units are in uisweep's `anchor_bases.json`, unverified, and four
TUs produced no candidate at all.  This tool replaces the score with two exact
tests and, where they disagree with a uisweep guess, they win.

METHOD 1 -- anchor on the LAST ClassDef class-name literal
----------------------------------------------------------
A ClassDef's class name is not just "some string": it is a C++ class name, and
every one of them appears in the engine's class list (`engine_classes.txt`, the
traceback-table census).  The pool is laid out as a class's keyword run
followed by that class's ClassDef name/super strings, so each ClassDef gives
one exact, checkable equation:

    base = addr(class_name_of_that_ClassDef) - offset_of_its_r4

Anchoring on the FIRST ClassDef that yields a class name reproduces uisweep's
15/15 on single-class TUs but still ties on multi-class ones.  Anchoring on the
**LAST** ClassDef in the TU and then requiring that the candidate also make
*every* ClassDef's class name and super name decode to a real class name (or
"" for a root), every event keyword a clean printable token, and every
argspec/argnames pair agree on arity, collapses the tie on every TU that
defines its own events.  TUs that only *bind* events another TU defined (the
picker / dialog / label files) define no argspec at all and can still tie.

METHOD 2 -- read the pool pointer straight out of the TOC
---------------------------------------------------------
`rBASE` comes from one `lwz rBASE, -OFF(r2)` per __sinit__, and no two TUs
share that slot.  Applying the PEF loader's relocations
(`pefdata.relocate_file`) turns the slot's contents into the absolute pool
address, so `base = slot_value - data_base` with nothing to search.  Validated
against every TU the anchor settles independently: 27/27 identical.

The two methods are combined: the anchor is the *proof*, the TOC slot is the
*tie-break*, and a base is only reported when at least one of them is exact --
with `method` saying which.  `--check` refuses anything the anchor did not
confirm.

Usage:
    python3 tools/ef2/ui_pool_base.py <tu> [<tu> ...]   # _cpp appended if missing
    python3 tools/ef2/ui_pool_base.py --all
    python3 tools/ef2/ui_pool_base.py --all --json OUT.json
    python3 tools/ef2/ui_pool_base.py --all --check     # anchor-only
"""
import json
import os
import re
import struct
import subprocess
import sys
import collections

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/work"
MAC = os.path.join(WORK, "mac_symbols")
DECOMP = os.path.join(MAC, "engine_decomp")
PEF = os.path.join(MAC, "macbin", "EF2_mac_engine.pef")
PPCDIS = os.path.join(MAC, "tools", "ppcdis.py")
CLASSES = os.path.join(MAC, "engine_classes.txt")

# ---------------------------------------------------------------------------
# The engine's string pool: the unpacked data section.  Addresses used through
# out are *section-relative* (== the index into this image), which is the same
# convention uistrings.txt and uisweep's anchor_bases.json use.
# ---------------------------------------------------------------------------
_syspath_added = False


def _pool():
    global IMG, _syspath_added
    try:
        return IMG
    except NameError:
        if not _syspath_added:
            sys.path.insert(0, os.path.join(MAC, "tools"))
            _syspath_added = True
        import pefdata
        _d, _c, _s, IMG, _st = pefdata.data_image(PEF)
        return IMG


def cstr(addr):
    """NUL-terminated string at a pool index, or None when out of range."""
    img = _pool()
    if addr is None or addr < 0 or addr >= len(img):
        return None
    e = img.find(b"\x00", addr)
    if e < 0:
        return None
    return img[addr:e].decode("latin1")


def is_printable(s):
    return s is not None and s != "" and all(32 <= ord(c) < 127 for c in s)


# The Event argspec alphabet.  Retail uilib emits at least s i f v e b d and the
# capitalised reference forms S F G (uiwidget's `aliascache` is 'ssSSSSSS', its
# `ordernumber` is 'd', `direction` is 'sF'); anything outside A-Z a-z on a
# candidate base means the offset landed in the middle of prose.
_ARGSPEC_RE = re.compile(r"^[A-Za-z]+$")


def argspec_matches_args(spec, names):
    """Retail writes one argname per argspec character ('ffffB' -> 'x y width
    height resize', 'ssSSSSSS' -> 8 names, 'd' -> 'num'), so a candidate base
    has to reproduce the count exactly.  This is the strongest single
    discriminator between competing bases: prose that happens to be a class
    name does not survive it."""
    if spec is None or names is None:
        return True
    if not spec or not names:
        return False
    if not _ARGSPEC_RE.match(spec):
        return False
    return len(spec) == len(names.split())


def class_names():
    """Set of every class name in the engine's traceback-table census."""
    global _CLASS_NAMES
    try:
        return _CLASS_NAMES
    except NameError:
        _CLASS_NAMES = set()
        for line in open(CLASSES, encoding="latin1"):
            p = line.split(None, 1)
            if len(p) == 2:
                _CLASS_NAMES.add(p[1].strip())
        return _CLASS_NAMES


def is_class_name(s):
    return s in class_names()


# ---------------------------------------------------------------------------
# The __sinit__ disassembly
# ---------------------------------------------------------------------------
def sinit_symbol(tu):
    """`__sinit__:<tu>` -> (address, size) out of the decomp file header."""
    p = os.path.join(DECOMP, "__sinit__%s.c" % tu)
    if not os.path.exists(p):
        return None
    with open(p) as f:
        m = re.search(r"@0x([0-9a-fA-F]+) size=(\d+)", f.readline())
    if not m:
        return None
    return int(m.group(1), 16), int(m.group(2))


def dis(addr, size):
    out = subprocess.run(
        [sys.executable, PPCDIS, "engine", "0x%08x" % addr, "--size", str(size)],
        capture_output=True, text=True, check=True).stdout
    ins = []
    for line in out.splitlines():
        m = re.match(r"\s+0x([0-9a-f]+)\s+(\S+)\s*(.*?)\s*(?:;\s*(.*?)\s*)?$", line)
        if m:
            ins.append((int(m.group(1), 16), m.group(2), m.group(3), m.group(4)))
    return ins


# ---------------------------------------------------------------------------
# The register walk.  `base` may be None, in which case pool references stay
# symbolic ("pool", off) and nothing is resolved -- that is what the anchor
# search runs on.
# ---------------------------------------------------------------------------
ARGS = set(range(3, 11))

# `addi rD, rBASE, IMM` -- the only pool-reference form the __sinit__ blocks use.
_RE_ADDI = re.compile(r"r(\d+),\s*r(\d+),\s*(0x[0-9a-f]+|\d+)$")
_RE_LWZ_TOC = re.compile(r"r(\d+),\s*(-?0x[0-9a-f]+)\(r2\)$")
_RE_LI = re.compile(r"r(\d+),\s*(-?\d+|0x[0-9a-f]+)$")
_RE_MR = re.compile(r"r(\d+),\s*r(\d+)$")


def base_register(ins):
    """The pool base register: the one most `addi rD, rS, IMM` reads."""
    n = collections.Counter()
    for _a, op, args, _c in ins:
        m = _RE_ADDI.match(args)
        if m:
            n[int(m.group(2))] += 1
    return max(n, key=lambda r: n[r]) if n else None


def _str_of(reg, r, base):
    """Resolve argument register `r`: either `pool + offset`, or a whole pointer
    the __sinit__ fetched from the TOC."""
    v = reg.get(r)
    if v is None:
        return None
    if v[0] == "pool":
        return cstr(None if base is None else base + v[1])
    if v[0] == "toc" and base is not None:
        return _toc_cstr(v[1])
    return None


def _toc_cstr(tocoff):
    """A string pointer some __sinit__ blocks load whole from the TOC instead of
    building as `pool + offset` (UIScrollView, UIAmmoColorLabel, ... do this for
    the ClassDef class name).  Resolved the same way the pool pointer is."""
    if _TOC_BASE is None:
        return None
    idx = _TOC_BASE + tocoff - _DATA_BASE
    if idx < 0 or idx + 4 > len(_MEM):
        return None
    return cstr(struct.unpack_from(">I", _MEM, idx)[0] - _DATA_BASE)


def walk(ins, base, basereg):
    """Run the walk; return (events, classdefs) with symbolic or resolved refs."""
    reg = {}
    events, classdefs = [], []

    def ref(r):
        v = reg.get(r)
        if v is None:
            return None
        if v[0] == "pool":
            return None if base is None else base + v[1]
        if v[0] == "toc" and base is not None:
            # a whole pointer fetched from the TOC; only resolvable once the
            # TOC is calibrated, so the anchor pass never sees one
            return None
        return None

    for a, op, args, cmt in ins:
        if op == "lwz":
            m = _RE_LWZ_TOC.match(args)
            if m:
                reg[int(m.group(1))] = ("toc", int(m.group(2), 0))
        elif op == "addi":
            m = _RE_ADDI.match(args)
            if m and int(m.group(2)) == basereg:
                reg[int(m.group(1))] = ("pool", int(m.group(3), 0))
        elif op == "li":
            m = _RE_LI.match(args)
            if m:
                reg[int(m.group(1))] = ("imm", int(m.group(2), 0))
        elif op == "mr":
            m = _RE_MR.match(args)
            if m and int(m.group(2)) in reg:
                reg[int(m.group(1))] = reg[int(m.group(2))]
        elif op != "bl":
            continue

        if cmt and "Event::Event" in cmt:
            events.append({
                "addr": a,
                "toc": reg.get(3, (None, None))[1],
                "name": _str_of(reg, 4, base),
                "type": reg.get(5, (None, None))[1],
                "argspec": cstr(ref(6)),
                "argnames": cstr(ref(7)),
                "desc": cstr(ref(8)),
            })
            for r in ARGS:
                reg.pop(r, None)
        elif cmt and "ClassDef::ClassDef" in cmt:
            sz = reg.get(9)
            classdefs.append({
                "addr": a,
                "cls": _str_of(reg, 4, base),
                "super": cstr(ref(6)),
                "size": sz[1] if sz and sz[0] == "imm" else None,
                # kept symbolic so the anchor search can read the raw offset
                "_cls_off": reg.get(4, (None, None))[1]
                if reg.get(4, (None, None))[0] == "pool" else None,
            })
            for r in ARGS:
                reg.pop(r, None)
    return events, classdefs


# ---------------------------------------------------------------------------
# Base recovery
# ---------------------------------------------------------------------------
def toc_slot(ins, basereg):
    """The r2-relative offset of the `lwz rBASE, -OFF(r2)` that loads rBASE."""
    for _a, op, args, _c in ins:
        m = _RE_LWZ_TOC.match(args)
        if m and int(m.group(1)) == basereg:
            return int(m.group(2), 0)
    return None


_MEM = None          # relocated data image
_DATA_BASE = None    # address the data section is mapped at
_TOC_BASE = None     # absolute address of r2, calibrated from the anchors
_RELOC_DONE = False


def _reloc():
    global _MEM, _DATA_BASE, _RELOC_DONE, _syspath_added
    if _RELOC_DONE:
        return
    if not _syspath_added:
        sys.path.insert(0, os.path.join(MAC, "tools"))
        _syspath_added = True
    import pefdata
    r = pefdata.relocate_file(PEF)
    _MEM, _DATA_BASE = r["mem"], r["data_base"]
    _RELOC_DONE = True


def _anchor_only(tu, ins, basereg):
    """METHOD 1.  Return the unique base the LAST-ClassDef anchor proves, or
    None.  Ties and dead ends are reported separately so the caller can fall
    back to the TOC slot rather than guessing."""
    _ev, cds = walk(ins, None, basereg)
    cds = [c for c in cds if c["_cls_off"] is not None]
    if not cds:
        return None, "no ClassDef takes its class name from the pool"
    off = cds[-1]["_cls_off"]

    candidates = []
    img = _pool()
    p = 0
    while p < len(img):
        e = img.find(b"\x00", p)
        if e < 0:
            break
        if e > p and is_class_name(img[p:e].decode("latin1")):
            candidates.append(p - off)
        p = e + 1

    scored, ties = [], 0
    for b in candidates:
        if b < 0:
            continue
        ev, cc = walk(ins, b, basereg)
        if not all(is_class_name(c["cls"]) for c in cc):
            continue
        if not all(c["super"] is None or c["super"] == "" or is_class_name(c["super"])
                   for c in cc):
            continue
        if not all(is_printable(e["name"]) for e in ev):
            continue
        if not all(argspec_matches_args(e["argspec"], e["argnames"]) for e in ev):
            continue
        printable = sum(1 for e in ev for f in ("argspec", "argnames", "desc")
                        if is_printable(e[f]))
        scored.append((printable, b))
    if not scored:
        return None, "no class-name candidate satisfies every ClassDef"
    scored.sort(reverse=True)
    if len([s for s in scored if s[0] == scored[0][0]]) > 1:
        return None, "tie between %d candidates" % len(
            [s for s in scored if s[0] == scored[0][0]])
    return scored[0][1], "anchored on last ClassDef (%s at +0x%x)" % (
        walk(ins, scored[0][1], basereg)[1][-1]["cls"], off)


def _slot_base(ins, basereg):
    """METHOD 2.  Read the pool pointer out of the TOC slot that loads rBASE.

    Needs the TOC base calibrated first (calibrate_toc), because the slot is
    addressed r2-relative and r2 sits inside the data section."""
    _reloc()
    off = toc_slot(ins, basereg)
    if off is None:
        return None, "rBASE is not loaded from the TOC"
    if _TOC_BASE is None:
        return None, "TOC base not calibrated"
    idx = _TOC_BASE + off - _DATA_BASE
    if idx < 0 or idx + 4 > len(_MEM):
        return None, "TOC slot out of range (r2%+#x)" % off
    return struct.unpack_from(">I", _MEM, idx)[0] - _DATA_BASE, \
        "read off the TOC slot (r2%+#x)" % off


def _decode_ok(tu, ins, basereg, base):
    """A base is only accepted if the whole TU decodes against it coherently."""
    ev, cc = walk(ins, base, basereg)
    if not cc:
        return False
    if not all(is_class_name(c["cls"]) for c in cc):
        return False
    if not all(c["super"] is None or c["super"] == "" or is_class_name(c["super"])
               for c in cc):
        return False
    if not all(is_printable(e["name"]) for e in ev):
        return False
    if not all(argspec_matches_args(e["argspec"], e["argnames"]) for e in ev):
        return False
    return True


def recover(tu):
    """Return {tu, base, method, ...} or {tu, error, ...}."""
    sym = sinit_symbol(tu)
    if not sym:
        return {"tu": tu, "error": "no __sinit__ decomp"}
    ins = dis(*sym)
    basereg = base_register(ins)
    if basereg is None:
        return {"tu": tu, "error": "no pool references"}

    calibrate_toc()
    anchor, why = _anchor_only(tu, ins, basereg)
    slot, why2 = _slot_base(ins, basereg)
    plausible = 0 <= slot < len(_pool()) if slot is not None else False

    if anchor is not None and slot is not None and plausible and anchor == slot:
        method = "anchor + TOC slot agree"
    elif anchor is not None and (slot is None or not plausible):
        method = "anchor only"
    elif anchor is None and slot is not None and plausible and _decode_ok(tu, ins, basereg, slot):
        method = "TOC slot (%s)" % why
    elif anchor is not None:
        method = "anchor only"
    else:
        return {"tu": tu, "error": "anchor: %s; TOC slot: %s"
                % (why, why2 if slot is None or not plausible else "base 0x%x decodes incoherently" % slot)}

    base = anchor if anchor is not None else slot
    ev, cc = walk(ins, base, basereg)
    return {"tu": tu, "base": base, "method": method,
            "slot_base": slot, "anchor_base": anchor,
            "events": ev, "classdefs": [{k: v for k, v in c.items()} for c in cc]}


def calibrate_toc():
    """Absolute r2, derived rather than assumed.

    The pool pointer lives in one TOC slot per TU, addressed `TOC + off`; its
    relocated *contents* are the absolute pool address `DATA_BASE + base`.  For
    a TU whose base the anchor already proved, every word in the image equal to
    `DATA_BASE + base` proposes a TOC value, and only the true TOC satisfies
    all TUs at once -- so the intersection pins it with no hand-derived
    constant anywhere.  Raises if the intersection is not a single value.
    """
    global _TOC_BASE
    if _TOC_BASE is not None:
        return _TOC_BASE
    _reloc()
    mem, db = _MEM, _DATA_BASE
    hits = None
    used = 0
    for tu in TUS:
        tu += "_cpp"
        sym = sinit_symbol(tu)
        if not sym:
            continue
        ins = dis(*sym)
        br = base_register(ins)
        if br is None:
            continue
        off = toc_slot(ins, br)
        if off is None:
            continue
        anchor, _why = _anchor_only(tu, ins, br)
        if anchor is None:
            continue
        want = db + anchor
        proposed = set()
        j = 0
        while True:
            j = mem.find(struct.pack(">I", want), j)
            if j < 0:
                break
            proposed.add(j + db - off)
            j += 1
        hits = proposed if hits is None else (hits & proposed)
        used += 1
    if not hits or len(hits) != 1 or used < 5:
        raise SystemExit("TOC base is not uniquely determined: %r from %d TUs"
                         % ([hex(h) for h in (hits or ())], used))
    _TOC_BASE = hits.pop()
    return _TOC_BASE


# ---------------------------------------------------------------------------
# The port side: Event definitions + CLASS_DECLARATION bindings
# ---------------------------------------------------------------------------
_DECL = re.compile(r'Event\s+(EV_\w+)\s*\(\s*(.*?)\s*\)\s*;', re.S)
_TOK = re.compile(r'NULL|"(?:[^"\\]|\\.)*"')
_CD = re.compile(r'CLASS_DECLARATION\s*\(\s*\w+\s*,\s*(\w+)\s*,[^)]*\)\s*\{', re.S)
_ROW = re.compile(r'\{\s*&\s*(EV_\w+)\s*,\s*&\w+::\s*(\w+)\s*\}')


def _unquote(t):
    if t == "NULL":
        return None
    return bytes(t[1:-1], "utf-8").decode("unicode_escape")


def port_dir(d):
    """(class, keyword) -> {argspec, argnames, desc, file, var} for a directory
    of the port's UI sources.  classinfo_*.c-style TUs are skipped."""
    out = {}
    for fn in sorted(os.listdir(d)):
        if not fn.endswith(".cpp"):
            continue
        src = open(os.path.join(d, fn), encoding="utf-8", errors="replace").read()
        events = {}
        for m in _DECL.finditer(src):
            t = _TOK.findall(m.group(2))
            if len(t) < 3:
                continue
            events[m.group(1)] = {
                "var": m.group(1), "file": fn, "kw": _unquote(t[0]),
                "argspec": _unquote(t[1]) if len(t) > 1 else None,
                "argnames": _unquote(t[2]) if len(t) > 2 else None,
                "desc": _unquote(t[3]) if len(t) > 3 else None,
            }
        for cm in _CD.finditer(src):
            cls = cm.group(1)
            tail = src[cm.end(): cm.end() + 20000]
            end = tail.find("{NULL,")
            if end < 0:
                end = len(tail)
            for rm in _ROW.finditer(tail[:end]):
                ev = events.get(rm.group(1))
                if ev:
                    out[(cls, ev["kw"])] = ev
    return out


def retail_by_class(tus):
    """(class, keyword) -> record, plus the set of classes those TUs define.

    Uses retail's own pool layout: within one __sinit__ a class's keyword run
    precedes its ClassDef, so an Event belongs to the first ClassDef at or after
    its ctor address."""
    out = {}
    classes = set()
    for tu in [t if t.endswith("_cpp") else t + "_cpp" for t in tus]:
        r = recover(tu)
        if "error" in r:
            continue
        classes.update(c["cls"] for c in r["classdefs"])
        cds = sorted(r["classdefs"], key=lambda c: c["addr"])
        for e in r["events"]:
            if e["name"] is None:
                continue
            owner = next((c["cls"] for c in cds if c["addr"] >= e["addr"]),
                         cds[-1]["cls"] if cds else None)
            if owner:
                out[(owner, e["name"])] = e
    return out, classes


def diff(tus, dirs):
    """Print the per-(class, keyword) divergences the corrected bases expose."""
    retail, classes = retail_by_class(tus)
    port = {}
    for d in dirs:
        port.update(port_dir(d))
    keys_r = {k for k in retail if k[0] in classes}
    keys_p = {k for k in port if k[0] in classes}
    changed = 0
    for k in sorted(keys_r & keys_p):
        r, p = retail[k], port[k]
        if (r["argspec"], r["argnames"], r["desc"]) == (p["argspec"], p["argnames"], p["desc"]):
            continue
        changed += 1
        what = [f for f, a, b in (("argspec", r["argspec"], p["argspec"]),
                                  ("argnames", r["argnames"], p["argnames"]),
                                  ("desc", r["desc"], p["desc"])) if a != b]
        print("\n[%s] %s :: %s   (%s : %s)" % ("/".join(what), k[0], k[1], p["file"], p["var"]))
        print("   retail argspec=%-10r argnames=%-30r desc=%r"
              % (r["argspec"], r["argnames"], r["desc"]))
        print("   PORT   argspec=%-10r argnames=%-30r desc=%r"
              % (p["argspec"], p["argnames"], p["desc"]))
    print("\nretail=%d port=%d common=%d changed=%d"
          % (len(keys_r), len(keys_p), len(keys_r & keys_p), changed))
    print("\n### retail-only (registered in retail, absent from the port at that class):")
    for k in sorted(keys_r - keys_p):
        r = retail[k]
        print("   %-26s %-28s argspec=%-8r argnames=%-24r" % (k[0], k[1], r["argspec"], r["argnames"]))
    print("\n### port-only (registered in the port, absent from retail at that class):")
    for k in sorted(keys_p - keys_r):
        p = port[k]
        print("   %-26s %-28s argspec=%-8r argnames=%-24r  (%s : %s)"
              % (k[0], k[1], p["argspec"], p["argnames"], p["file"], p["var"]))
    return retail, port


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------
TUS = [
    "uiwidget", "uibutton", "uibindbutton", "uicheckbox", "uiconsole",
    "uifield", "uifloatwnd", "uilabel", "uilayout", "uilist", "uilistbox",
    "uilistctrl", "uimenu", "uimledit", "uimove", "uinotepad", "uipopupmenu",
    "uipulldownmenu", "uipulldownmenucontainer", "uiradio", "uirow",
    "uirowcolumn", "uiscrollable", "uiscrollshader", "uiscrollview",
    "uislider", "uistatus", "uiswitchbox", "uitable", "uivertscroll",
    "uiwinman", "ui_init",
    "cl_uistd", "cl_uiview3d", "cl_radardialog", "cl_inventorybar",
    "cl_ammocolorlabel", "cl_colorstatlabel", "cl_randomlabel",
    "cl_uidialoghead", "cl_uidialogtext", "cl_uifilepicker",
    "cl_uiloadsave", "cl_uimaprunner", "cl_uiminicon", "cl_uirendermodel",
    "cl_uiserverlist", "cl_uisoundpicker", "cl_ui", "cl_inv", "cl_invrender",
    "cl_bindcommands", "cl_systeminfo", "ucolor", "uicolor", "console",
]

# uisweep_0908.md section 5: these had no verified base.  They are the ones this
# tool exists for.  All 21 now resolve.
UNVERIFIED = [
    "uibindbutton", "uicheckbox", "uifield", "uilabel", "uilist",
    "uilistctrl", "uimledit", "uinotepad", "uipopupmenu", "uiradio",
    "uiscrollview", "uistatus", "uiswitchbox", "uirowcolumn",
    "cl_ammocolorlabel", "cl_radardialog", "cl_randomlabel",
    "cl_uidialogtext", "cl_uirendermodel", "cl_uistd", "console_cpp",
]

# uisweep's unverified guesses (its `anchor_bases.json`, docs/wt-uisweep-0908).
SWEEP_GUESS = {
    "cl_radardialog_cpp": 0x037618, "cl_randomlabel_cpp": 0x037988,
    "cl_uidialogtext_cpp": 0x03AA90, "cl_uirendermodel_cpp": 0x03BA80,
    "cl_uistd_cpp": 0x03D47E, "console_cpp": 0x03AD35,
    "uibindbutton_cpp": 0x017214, "uicheckbox_cpp": 0x017DC8,
    "uifield_cpp": 0x0189C8, "uilabel_cpp": 0x03DA8F,
    "uilist_cpp": 0x0194C0, "uilistctrl_cpp": 0x03C743,
    "uimledit_cpp": 0x01AFE6, "uinotepad_cpp": 0x01AC60,
    "uirowcolumn_cpp": 0x01C980, "uiswitchbox_cpp": 0x01DD88,
}


def main(argv):
    if "--validate" in argv:
        tus = [t for t in TUS if t not in [u[:-4] for u in UNVERIFIED]]
    else:
        names = [a for a in argv if not a.startswith("-")]
        # drop the value of any option that takes one
        for opt in ("--diff", "--json"):  # options that take a value
            if opt in names:
                names.remove(opt)
            if opt in argv:
                val = argv[argv.index(opt) + 1]
                if val in names:
                    names.remove(val)
        if "--all" in argv or not names:
            tus = TUS
        else:
            tus = names
    tus = [t if t.endswith("_cpp") else t + "_cpp" for t in tus]

    anchor_only = "--check" in argv
    rows = []
    for tu in tus:
        r = recover(tu)
        if anchor_only and "base" in r and "TOC" in r["method"] \
                and "anchor" not in r["method"]:
            r = {"tu": tu, "error": "anchor alone could not settle this TU"}
        rows.append(r)
        if "error" in r:
            print("# %-34s %s" % (tu, r["error"]))
            continue
        guess = SWEEP_GUESS.get(tu)
        flag = ""
        if guess is None:
            flag = ""
        elif guess != r["base"]:
            flag = "  SWEEP-GUESS-0x%06x-WRONG" % guess
        else:
            flag = "  sweep agreed"
        print("# %-34s base=0x%06x  [%s]%s"
              % (tu, r["base"], r["method"], flag))
        for c in r["classdefs"]:
            print("    CLASS %-26s super=%-24s size=%s"
                  % (c["cls"], c["super"], c["size"]))
        for e in r["events"]:
            print("    toc%-6s %-28s type=%-4s argspec=%-8r argnames=%-24r desc=%r"
                  % (e["toc"], e["name"], e["type"], e["argspec"],
                     e["argnames"], e["desc"]))

    if "--json" in argv:
        out = [r for r in rows if "base" in r]
        path = argv[argv.index("--json") + 1]
        json.dump({r["tu"]: {"base": r["base"], "method": r["method"],
                             "classdefs": r["classdefs"], "events": r["events"]}
                   for r in out}, open(path, "w"), indent=1)
        print("# wrote %s" % path)
    if "--diff" in argv:
        dirs = []
        for i, a in enumerate(argv):
            if a == "--diff" and i + 1 < len(argv):
                dirs.append(argv[i + 1])
        diff(tus, dirs)
        return 0
    ok = [r for r in rows if "base" in r]
    bad = [r for r in rows if "error" in r]
    print("\n%d/%s resolved, %d failed" % (len(ok), len(rows), len(bad)))
    for r in bad:
        print("  FAILED %s: %s" % (r["tu"], r["error"]))
    return 0 if not bad else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
