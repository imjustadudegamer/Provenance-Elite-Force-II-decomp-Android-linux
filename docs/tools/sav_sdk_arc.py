#!/usr/bin/env python3
"""Extract ordered Class::Archive field lists out of the real EF2 SDK source.

Every rule this emits is read straight out of docs/evidence/ef2-sdk/ef2-sdk/... -- the
retail game source Ritual shipped -- so it is the format ground truth, not a
guess.  The output feeds sav_stream.CLASS_FIELDS.

The emitter understands the shapes the SDK actually uses:

  arc.ArchiveInteger/Unsigned/Byte/Char/Short/UnsignedShort/Float/Double/
      Boolean/Bool/Vector/Vec3/Vec4/Quat/String( &field )
  arc.ArchiveRaw( &f, sizeof( f ) ) / arc.ArchiveRaw( f, N )
  arc.ArchiveEnum( f, ENUMTYPE )            == one int
  arc.ArchiveObjectPointer / ArchiveSafePointer / ArchiveEvent /
      ArchiveEventPointer / ArchiveObject / ReadObject
  if ( arc.Saving() ) { A } else { B }      -> take A (A and B archive the
                                               same fields in the same order;
                                               one arm is the write side and
                                               one the read side)
  if ( arc.Loading() )  { A } else { B }    -> take B
  for ( i = a; i <= N; i++ ) { ... }        -> repeat N times
  x.Archive( arc ) where x is a Container   -> the archive.h Container<T>
                                               specialisations

Anything else becomes an 'opaque' marker, and any class whose token list
contains an opaque marker is NOT used for field walking -- the walker falls
back to the object's own size prefix.  sav_stream.py --verify then checks every
modelled class against every corpus instance, so a wrong transcription can
never silently pass.
"""
import os
import re
import sys

SDK_ROOT_CANDIDATES = [
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + "/docs/evidence/ef2-sdk/ef2-sdk",
]

# --------------------------------------------------------------- C++ scanner


def _strip_comments(text):
    """Single-pass // and /* */ stripper.

    A naive two-pass version breaks on the SDK's `//**********` banner rules:
    the `/*` inside a line comment opens a block comment that swallows the rest
    of the file (entity.cpp loses Entity::Archive that way), so the two comment
    kinds have to be tokenised together.
    """
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            q = c
            j = i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == q:
                    j += 1
                    break
                j += 1
            out.append(text[i:j])
            i = j
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            i = n if j < 0 else j
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            out.append(" ")
            continue
        out.append(c)
        i += 1
    return "".join(out)


def brace_body(text, open_idx):
    """Given the index of '{', return (body, index_after_close)."""
    depth = 0
    i = open_idx
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            q = c
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == q:
                    i += 1
                    break
                i += 1
            continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return text[open_idx + 1:i], i + 1
        i += 1
    raise ValueError("unbalanced braces")


def iter_matching(src, needle_re):
    """Yield (match, body, start_of_signature) for `needle_re` whose body is a
    brace block."""
    for m in re.finditer(needle_re, src):
        b = src.find("{", m.end() - 1)
        if b < 0:
            continue
        head = src[max(0, m.start() - 400):m.start()]
        try:
            body, _ = brace_body(src, b)
        except ValueError:
            continue
        yield m, body, head


# --------------------------------------------------------------- token model
# (kind, name, arg)


def _simple(kind):
    return lambda stmt, ctx: [(kind, None, None)]


CALL_MAP = {
    "ArchiveInteger": ("int",),
    "ArchiveUnsigned": ("uint",),
    "ArchiveByte": ("byte",),
    "ArchiveChar": ("char",),
    "ArchiveShort": ("short",),
    "ArchiveUnsignedShort": ("ushort",),
    "ArchiveFloat": ("float",),
    "ArchiveDouble": ("double",),
    "ArchiveBoolean": ("boolean",),
    "ArchiveBool": ("bool1",),
    "ArchiveVector": ("vec",),
    "ArchiveVec3": ("vec3",),
    "ArchiveVec4": ("vec4",),
    "ArchiveQuat": ("quat",),
    "ArchiveString": ("string",),
    "ArchiveSafePointer": ("safeptr",),
    "ArchiveObjectPointer": ("objptr",),
    "ArchiveEvent": ("event",),
    "ArchiveEnum": ("int",),
}


class Unsupported(Exception):
    pass


def _split_args(argstr):
    """Split a C++ argument list on top-level commas."""
    out, depth, cur, i = [], 0, "", 0
    while i < len(argstr):
        c = argstr[i]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == "," and depth == 0:
            out.append(cur)
            cur = ""
            i += 1
            continue
        cur += c
        i += 1
    if cur.strip():
        out.append(cur)
    return [a.strip() for a in out]


def _field_of(arg):
    m = re.match(r"^[&(]*\s*([A-Za-z_]\w*)", arg)
    return m.group(1) if m else None


def _last_ident(arg):
    """The right-most identifier of an lvalue: `_connections[i]->maxheight`
    -> `maxheight`, which is the name the sizeof() operand names."""
    ids = re.findall(r"[A-Za-z_]\w*", arg)
    return ids[-1] if ids else None


def _raw_size(argstr):
    args = _split_args(argstr)
    if len(args) != 2:
        raise Unsupported("ArchiveRaw with %d args" % len(args))
    sz = args[1].strip()
    m = re.match(r"^sizeof\(\s*[&A-Za-z_]*(\w*)\s*\)$", sz)
    if m:
        # sizeof( &field ) / sizeof( field ) / sizeof( type )
        return ("named", m.group(1) or _last_ident(args[0]))
    m = re.match(r"^(\d+)$", sz)
    if m:
        return ("lit", int(m.group(1)))
    m = re.match(r"^sizeof\(\s*[&A-Za-z_]*\w*\s*\)\s*\*\s*([A-Za-z_]\w*)$",
                 sz)
    if m:
        # sizeof( field[0] ) * count -- count is on the wire next to it
        return ("scaled", m.group(1))
    if re.match(r"^sizeof\(", sz):
        return ("named", _last_ident(args[0]))
    raise Unsupported("ArchiveRaw size %r" % sz)


# statements that do NOT touch the stream and are safe to skip
_SKIP_RE = re.compile(
    r"^(?:Class::Archive|Listener::Archive)\s*\(\s*arc\s*\)$")


def parse_block(body, ctx, out):
    """Parse a brace block into `out`.  Raises Unsupported on anything the
    emitter does not model -- the caller then drops that class entirely."""
    i = 0
    n = len(body)
    while i < n:
        c = body[i]
        if c.isspace():
            i += 1
            continue
        if c == "{":
            inner, i = brace_body(body, i)
            parse_block(inner, ctx, out)
            continue
        # take one statement up to the next ';' at depth 0 (or a control head)
        j = i
        depth = 0
        while j < n:
            ch = body[j]
            if ch in "([{":
                depth += 1
            elif ch in ")]}":
                depth -= 1
            elif ch == ";" and depth == 0:
                break
            j += 1
        stmt = body[i:j].strip()
        nxt = j + 1
        if not stmt:
            i = nxt
            continue
        # ---- control flow -----------------------------------------------
        m = re.match(r"^if\s*", stmt, re.S)
        if m and _skip_ws(body, i + 2) < n and body[_skip_ws(body, i + 2)] == "(":
            pstart = _skip_ws(body, i + 2)
            cond, after = _take_parens(body, pstart)
            k = _skip_ws(body, after)
            if k < n and body[k] == "{":
                then_body, k2 = brace_body(body, k)
                then_single = None
            else:
                # `if ( cond ) one_statement;` -- retail uses this for
                # setModel / gi.archetypeindex side effects after an
                # ArchiveString.  Parse the single statement; if it touches the
                # stream the parse will raise and the class is dropped.
                j2 = k
                depth2 = 0
                while j2 < n:
                    ch = body[j2]
                    if ch in "([{":
                        depth2 += 1
                    elif ch in ")]}":
                        depth2 -= 1
                    elif ch == ";" and depth2 == 0:
                        break
                    j2 += 1
                then_body, then_single = body[k:j2], body[k:j2]
                k2 = j2 + 1
            k2 = _skip_ws(body, k2)
            else_body = None
            if body[k2:k2 + 4] == "else":
                k3 = _skip_ws(body, k2 + 4)
                if body[k3] == "{":
                    else_body, k2 = brace_body(body, k3)
                elif body[k3:k3 + 2] == "if":
                    # `else if (...)` chain: splice the nested if as the arm
                    nest_end = k3
                    depthn = 0
                    j3 = k3
                    seen_paren = False
                    while j3 < n:
                        if body[j3] == "(":
                            seen_paren = True
                            depthn += 1
                        elif body[j3] == ")":
                            depthn -= 1
                            if seen_paren and depthn == 0:
                                break
                        j3 += 1
                    cond2, after2 = _take_parens(body, j3)
                    k4 = _skip_ws(body, after2)
                    if body[k4] == "{":
                        else_body, k2 = brace_body(body, k4)
                    else:
                        raise Unsupported("nested else-if without a block")
                else:
                    # single-statement else arm
                    j3 = k3
                    depth3 = 0
                    while j3 < n:
                        ch3 = body[j3]
                        if ch3 in "([{":
                            depth3 += 1
                        elif ch3 in ")]}":
                            depth3 -= 1
                        elif ch3 == ";" and depth3 == 0:
                            break
                        j3 += 1
                    else_body = body[k3:j3]
                    k2 = j3 + 1
            pick = _pick_arm(cond)
            if pick == "both":
                subs = []
                for arm in (then_body, else_body):
                    sub = []
                    if arm is not None:
                        parse_block(arm, ctx, sub)
                    subs.append(sub)
                # `arc.ArchiveBool( &f ); if ( f ) arc.ArchiveString( &g );`
                # -- Program::Archive's string table.  The predicate is itself
                # on the wire, so the group is walkable: one byte then,
                # if non-zero, one string.
                kinds = [[t[0] for t in sub] for sub in subs]
                if kinds[0] == ["string"] and kinds[1] == []:
                    out.append(("condstring", _field_of(cond), None))
                elif kinds[0] == [] and kinds[1] == ["string"]:
                    out.append(("condstring", _field_of(cond), None))
                elif any(kinds):
                    raise Unsupported(
                        "stream ops in an arc-free if/else: %s" % cond[:60])
            else:
                chosen = {"then": then_body, "else": else_body}[pick]
                other = {"then": else_body, "else": then_body}[pick]
                if chosen is not None:
                    sub = []
                    try:
                        parse_block(chosen, ctx, sub)
                    except Unsupported:
                        # Both arms archive the same fields; retail sometimes
                        # writes one arm with a pointer walk it counts first
                        # (Program::Archive's type/def lists).  If the other
                        # arm parses to a counted loop over the same `num`,
                        # that is the same field list -- use it.
                        sub2 = []
                        if other is None:
                            raise
                        parse_block(other, ctx, sub2)
                        sub = sub2
                    out.extend(sub)
            i = k2
            continue
        if re.match(r"^for\s*", stmt, re.S):
            pstart = _skip_ws(body, i + 3)
            hdr, after = _take_parens(body, pstart)
            k = _skip_ws(body, after)
            if body[k] == "{":
                loop_body, k2 = brace_body(body, k)
            else:
                j4 = k
                depth4 = 0
                while j4 < n:
                    ch4 = body[j4]
                    if ch4 in "([{":
                        depth4 += 1
                    elif ch4 in ")]}":
                        depth4 -= 1
                    elif ch4 == ";" and depth4 == 0:
                        break
                    j4 += 1
                loop_body, k2 = body[k:j4], j4 + 1
            try:
                count = _loop_count(hdr)
            except Unsupported:
                # A pointer-walk loop (`for ( curtype = types; curtype;
                # curtype = curtype->next )`) only computes a count; it is
                # usable iff its body touches no stream ops.
                count = None
            sub = []
            parse_block(loop_body, ctx, sub)
            if count is None:
                if sub:
                    raise Unsupported("uncounted for-loop with stream ops")
            else:
                out.append(("repeat", None, (count, sub)))
            i = k2
            continue
        if re.match(r"^(?:while|switch|do)\b", stmt):
            raise Unsupported("unhandled control flow: %s" % stmt[:60])
        if _SKIP_RE.match(stmt):
            i = nxt
            continue
        # ---- a hand-written helper: G_ArchiveEdict( arc, edict ) --------
        m = re.match(r"^([A-Za-z_]\w*)\s*\(\s*arc\s*,", stmt)
        if m and m.group(1) in ctx.get("helpers", {}):
            out.extend(ctx["helpers"][m.group(1)])
            i = nxt
            continue
        # ---- an arc.ArchiveXxx(...) call -------------------------------
        got = _parse_call(stmt, ctx)
        if got is None:
            # harmless non-stream statement (num = ..., var = ..., decls)
            if re.search(r"\b(arc|Archiver)\b", stmt):
                raise Unsupported("unhandled stream statement: %s" % stmt[:90])
        else:
            out.extend(got)
        i = nxt


def _skip_ws(b, i):
    n = len(b)
    while i < n and b[i].isspace():
        i += 1
    return i


def _take_parens(b, i):
    """b[i] is '('; return (contents, index_after_close)."""
    depth = 0
    n = len(b)
    j = i
    start = i
    while j < n:
        if b[j] == "(":
            depth += 1
        elif b[j] == ")":
            depth -= 1
            if depth == 0:
                return b[start + 1:j], j + 1
        j += 1
    raise Unsupported("unbalanced parens")


def _pick_arm(cond):
    """Return which arm of an if/else archives the fields (the saving side)."""
    c = re.sub(r"\s+", "", cond)
    if re.match(r"^arc\.Saving\(\)$", c):
        return "then"
    if re.match(r"^arc\.Loading\(\)$", c):
        return "else"
    # retail has two hand-written guards the emitter special-cases:
    #   PathManager::Archive   -- `if ( ai_debuginfo->integer )` guards a debug
    #                             dump that is off in every shipped build
    #   ScriptMaster::Archive  -- `if ( vmStudio )` / `else` around the same
    #                             field set in both arms
    #   Program::Archive       -- `if ( strings[i].inuse )` inside a fixed-size
    #                             table walk
    # A compound `arc.Loading() && <side effect>` guard also takes the else arm.
    if "ai_debuginfo" in c or "vmStudio" in c:
        return "else"
    if "arc.Loading()" in c and not re.match(r"^!?arc\.Loading\(\)$", c):
        return "else"
    if "arc.Saving()" in c and not re.match(r"^!?arc\.Saving\(\)$", c):
        return "then"
    if re.match(r"^!?arc\.Saving\(\)", c) or re.match(r"^!?arc\.Loading\(\)", c):
        neg = c.startswith("!")
        base_save = "arc.Saving()" in c
        if base_save:
            return "else" if neg else "then"
        base_load = "arc.Loading()" in c
        if base_load:
            return "then" if neg else "else"
    # A condition that never mentions `arc` guards a non-stream choice -- e.g.
    # G_ArchiveEdict's `if ( edict->client )`.  "both" means: parse both arms,
    # and require that neither touches the stream.
    if "arc." not in c:
        return "both"
    raise Unsupported("cannot pick an arm for condition %r" % cond[:80])


def _loop_count(hdr):
    """`int i = 0; i < N; i++` -> N."""
    parts = [p.strip() for p in hdr.split(";")]
    if len(parts) != 3:
        raise Unsupported("odd for-header %r" % hdr[:70])
    cond, step = parts[1], parts[2]
    m = re.match(r"^i\s*([<>=!]+)=?\s*(.+)$", cond)
    if not m:
        raise Unsupported("odd for-condition %r" % cond[:60])
    op, bound = m.group(1), m.group(2).strip()
    if not re.match(r"^(?:i\s*\+\+|\+\+\s*i|i\s*\+=\s*1)$", step):
        raise Unsupported("odd for-step %r" % step[:40])
    if op == "<=":
        return ("expr", bound, 1)
    if op == "<":
        return ("expr", bound, 0)
    raise Unsupported("for operator %r" % op)


def _parse_call(stmt, ctx):
    """Return a token list, or None if the statement does not touch the stream."""
    s = re.sub(r"\s+", " ", stmt).strip()
    m = re.match(r"^(?:arc|Archiver)\s*\.\s*(\w+)\s*\((.*)\)$", s, re.S)
    if m:
        name, args = m.group(1), m.group(2)
        if name in CALL_MAP:
            kind = CALL_MAP[name]
            if kind == ("int",) and name == "ArchiveEnum":
                pass
            fld = _field_of(args)
            return [(kind[0], fld, None)]
        if name == "ArchiveRaw":
            how, v = _raw_size(args)
            if how == "lit":
                return [("raw", _field_of(_split_args(args)[0]), v)]
            if how == "scaled":
                return [("rawscaled", _last_ident(args[0]), v)]
            return [("rawsizeof", _last_ident(args[0]), v)]
        if name == "ArchiveObject":
            return [("object", _field_of(args), None)]
        if name == "ReadObject":
            return [("readobject", None, None)]
        if name == "ArchiveEventPointer":
            return [("eventptr", _field_of(args), None)]
        raise Unsupported("unhandled arc.%s" % name)
    # ArchiveEnum( field, TYPE )  -- fgame/archive.h:450-457 expands to
    # `int tempInt = (int)(thing); arc.ArchiveInteger( &tempInt ); ...`,
    # i.e. exactly one int32 on the wire.
    m = re.match(r"^ArchiveEnum\s*\(", s)
    if m:
        return [("int", _field_of(s[m.end():]), None)]
    # x.Archive( arc )  -- a Container<T> or a nested archived member
    m = re.match(r"^([A-Za-z_]\w*)\s*\.\s*Archive\s*\(\s*arc\s*\)$", s)
    if m:
        fld = m.group(1)
        return [("member", fld, ctx.get("decl", {}).get(fld))]
    # x->Archive( arc ) -- an embedded pointer member whose pointee class has
    # its own Archive (BindInfo / MorphInfo in Entity::Archive).  The pointee's
    # fields are spliced inline; retail writes them unconditionally when the
    # pointer is live.
    m = re.match(r"^([A-Za-z_]\w*)\s*->\s*Archive\s*\(\s*arc\s*\)$", s)
    if m:
        return [("member", m.group(1), ctx.get("decl", {}).get(m.group(1)))]
    # BaseClass::Archive( arc ) -- splice the base class's field list in place
    m = re.match(r"^([A-Za-z_]\w*)\s*::\s*Archive\s*\(\s*arc\s*\)$", s)
    if m:
        return [("base", m.group(1), None)]
    return None


# --------------------------------------------------------------- class table


def collect_sdk_sources():
    for root in SDK_ROOT_CANDIDATES:
        if not os.path.isdir(root):
            continue
        for dirpath, _, files in os.walk(root):
            if os.sep + "mac_symbols" in dirpath:
                continue
            for f in files:
                if f.endswith((".cpp", ".h", ".hpp")):
                    yield os.path.join(dirpath, f)


DECL_RE = re.compile(r"^\s*(?:const\s+)?"
                     r"((?:unsigned\s+)?[A-Za-z_][\w:<>,\s*&]*?)\s+"
                     r"([A-Za-z_]\w*)\s*(?:\[[^]]*\])?\s*;", re.S)


def class_decls(body):
    """Best-effort map local-var-name -> declared type, for Container<T>
    detection."""
    d = {}
    for stmt in body.split(";"):
        s = re.sub(r"\s+", " ", stmt).strip()
        m = re.match(r"^(Container<[^>]*>|SafePtr<[^>]*>|str|int|float|qboolean"
                     r"|bool|Vector|Quat|byte|char|short|unsigned|double)"
                     r"(?:\s*\*)?\s+([A-Za-z_]\w*)$", s)
        if m:
            d[m.group(2)] = m.group(1)
    return d


def extract(sdk_root):
    sources = {}
    for p in collect_sdk_sources():
        try:
            t = open(p, encoding="latin1").read()
        except OSError:
            continue
        if "\r\n" in t:
            t = t.replace("\r\n", "\n")
        sources[p] = _strip_comments(t)
    defines = _scan_defines(sources)
    decls = _member_decls(sources)
    classes = {}
    notes = []
    helpers = _helper_fns(sources, defines)
    # out-of-line definitions:  void FOO::Archive( Archiver &arc ) { ... }
    pat = re.compile(r"\bvoid\s+([A-Za-z_]\w*)\s*::\s*Archive\s*"
                     r"\(\s*Archiver\s*&\s*arc\s*\)")
    inline_pat = re.compile(r"\b(?:void|inline\s+void)\s+([A-Za-z_]\w*)\s*::\s*"
                            r"Archive\s*\(\s*(?:Archiver\s*&)?\s*arc\s*\)")
    for p, src in sources.items():
        for pat_ in (pat, inline_pat):
            for m, body, _head in iter_matching(src, pat_):
                cname = m.group(1)
                if cname in ("Container", "SafePtr", "str"):
                    continue
                try:
                    ctx = {"decl": decls.get(cname, {}), "defines": defines,
                           "helpers": helpers}
                    out = []
                    parse_block(body, ctx, out)
                except Unsupported as exc:
                    notes.append("%s (%s): %s" % (cname, os.path.basename(p),
                                                  exc))
                    continue
                if ("opaque", None, None) in out:
                    continue
                prev = classes.get(cname)
                if prev is None or _unresolved(prev) > _unresolved(out):
                    classes[cname] = (out, os.path.relpath(p, sdk_root))
    classes = _inline_bases(classes, notes)
    return classes, notes


# member declarations:  Container<X>  name;   /   str name;   /
#                      Animate *animate;   /   cplane_t groundplane;
_DECL_RE = re.compile(
    r"(?<![\w>])"
    r"([A-Za-z_]\w*(?:::[A-Za-z_]\w*)?"
    r"(?:<\s*[A-Za-z_]\w*\s*\*?\s*>)?)"
    r"(?:\s*\*\s*|\s+)"
    r"([A-Za-z_]\w*)"
    r"\s*(\[\s*(\w+)\s*\])?\s*;", re.S)

# words that begin statements, never declarations
_NOT_TYPES = frozenset([
    "return", "else", "case", "new", "delete", "sizeof", "const", "if", "for",
    "while", "switch", "do", "goto", "break", "continue", "catch", "throw",
    "assert", "int", "float", "bool", "char", "short", "double", "unsigned",
    "signed", "void", "static", "virtual", "inline", "operator", "public",
    "private", "protected", "enum", "typedef", "class", "struct", "true",
    "false", "NULL",
])


def _member_decls(sources):
    """classname -> {membername: declared type}"""
    out = {}
    # find each `class FOO ... { ... };` block so members stay with their class
    cls_re = re.compile(r"\bclass\s+([A-Za-z_]\w*)[^;{()]*\{")
    for src in sources.values():
        for m in cls_re.finditer(src):
            cname = m.group(1)
            b = src.find("{", m.end() - 1)
            try:
                body, _ = brace_body(src, b)
            except ValueError:
                continue
            d = out.setdefault(cname, {})
            for dm in _DECL_RE.finditer(body):
                ty, nm = dm.group(1), dm.group(2)
                arr, arrn = dm.group(3), dm.group(4)
                if ty in _NOT_TYPES or nm in _NOT_TYPES:
                    continue
                d[nm] = "%s[%s]" % (ty, arrn) if arr else ty
    return out


def _inline_bases(classes, notes):
    """Replace ('base', X, None) with X's own tokens, transitively."""
    resolved = {}

    def resolve(name, stack):
        if name in resolved:
            return resolved[name]
        if name not in classes or name in stack:
            return None
        toks, src = classes[name]
        out = []
        for t in toks:
            if t[0] == "base":
                sub = resolve(t[1], stack | {name})
                if sub is None:
                    notes.append("%s: cannot inline base %s" % (name, t[1]))
                    return None
                out.extend(sub)
            else:
                out.append(t)
        resolved[name] = out
        return out

    for name in list(classes):
        resolve(name, set())
    out = {}
    for name in classes:
        if name in resolved:
            out[name] = (resolved[name], classes[name][1])
    return out


def _unresolved(toks):
    n = 0
    for t in toks:
        if not isinstance(t, tuple) or len(t) < 3:
            continue
        if t[0] == "member" and t[2] is None:
            n += 1
        elif t[0] == "base":
            n += 1
    return n


# ---------------------------------------------------------------------------
# member resolution -- fgame/archive.h:212-446 Container<T> specialisations
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# struct sizes needed by `arc.ArchiveRaw( f, sizeof( f ) )`.
# Each is computed by hand from the SDK header cited, on the 32-bit PPC model.
# ---------------------------------------------------------------------------
BASE_SIZE = {"int": 4, "unsigned": 4, "float": 4, "double": 8,
             "qboolean": 4, "bool": 1, "char": 1, "byte": 1,
             "short": 2, "unsignedshort": 2, "long": 4, "Vector": 12,
             "Quat": 16, "vec3_t": 12, "vec4_t": 16, "str": 8}

# q_shared.h:938-941  typedef struct { vec3_t origin; vec3_t axis[3]; } orientation_t;
#   = 12 + 3*12                                  = 48
# q_shared.h:880-886  typedef struct cplane_s { vec3_t normal; float dist;
#                                               byte type; byte signbits;
#                                               byte pad[2]; } cplane_t;
#   = 12 + 4 + 1 + 1 + 2                         = 20
STRUCT_SIZES = {
    "orientation": 48,     # Entity::Archive -- arc.ArchiveRaw( orientation, ... )
    "groundplane": 20,     # Entity::Archive -- arc.ArchiveRaw( &groundplane, ... )
    # navigate.h:74 `byte maxheight[NUM_WIDTH_VALUES];` with navigate.h:59
    # `#define NUM_WIDTH_VALUES 16` -> 1 * 16, archived by
    # PathNodeConnection::Archive / PathNode::Archive
    "maxheight": 16,
}
# sizes only reachable through a member's own type (decl `T name[N]`)
ARRAY_SIZE = {}


def _struct_sizes(sources):
    """name -> byte size, for POD C structs in the SDK headers (32-bit PPC)."""
    sizes = {}
    prim = {"char": 1, "byte": 1, "unsigned char": 1, "signed char": 1,
            "short": 2, "unsigned short": 2, "unsigned short int": 2,
            "int": 4, "unsigned": 4, "unsigned int": 4, "long": 4,
            "unsigned long": 4, "float": 4, "double": 8, "qboolean": 4,
            "qboolean_t": 4, "bool": 1, "size_t": 4, "intptr_t": 4}
    # a few typedefs the SDK defines in terms of others
    alias = {"vec3_t": ("vec_t", 3), "vec4_t": ("vec_t", 4), "vec_t": None}

    # scan typedef struct { ... } NAME;  and  struct NAME { ... };
    for src in sources.values():
        for m in re.finditer(r"typedef\s+struct\s*(?:::)?[^;{]*\{", src):
            b = src.find("{", m.end() - 1)
            try:
                body, after = brace_body(src, b)
            except ValueError:
                continue
            nm = re.match(r"\s*([A-Za-z_]\w*)\s*;", src[after:after + 60])
            if not nm:
                continue
            sizes[nm.group(1)] = body
        for m in re.finditer(r"\bstruct\s+([A-Za-z_]\w*)\s*(?:::[^;{]*)?\{", src):
            b = src.find("{", m.end() - 1)
            try:
                body, _ = brace_body(src, b)
            except ValueError:
                continue
            sizes.setdefault(m.group(1), body)

    def size_of_type(t, depth=0):
        t = re.sub(r"\s+", " ", t).strip()
        if depth > 8:
            return None
        if t in prim:
            return prim[t]
        if t in ("vec_t", "float"):
            return 4
        low = t.lower()
        if low in prim:
            return prim[low]
        if t in sizes:
            return size_of_struct(t, depth)
        return None

    def size_of_struct(name, depth=0):
        if depth > 8:
            return None
        body = sizes.get(name)
        if body is None:
            return None
        total = 0
        # strip nested braces (unions / nested structs are rare here)
        for line in re.split(r";", body):
            line = line.strip()
            if not line or line.startswith("typedef"):
                continue
            m = re.match(r"^(.*?)([A-Za-z_]\w*)\s*(\[[^]]*\])?$", line, re.S)
            if not m:
                continue
            decl, arr = m.group(1), m.group(3)
            base = decl
            cnt = 1
            if arr:
                inner = re.match(r"\[\s*(\w+)\s*\]", arr)
                if not inner:
                    return None
                n = inner.group(1)
                cnt = int(n) if n.isdigit() else None
                if cnt is None:
                    return None
            # strip trailing pointer markers and leading const/unsigned words
            base = re.sub(r"\s*\*\s*$", "", base)
            if base.endswith("*"):
                total += 4 * cnt
                continue
            base = re.sub(r"^(?:const|mutable|unsigned|signed)\s+", "", base)
            if base in alias and alias[base]:
                total += prim["float"] * alias[base][1] * cnt
                continue
            s = size_of_type(base, depth + 1)
            if s is None:
                return None
            total += s * cnt
        return total

    out = {}
    for name in sizes:
        try:
            s = size_of_struct(name)
        except RecursionError:
            s = None
        if s:
            out[name] = s
    return out


def resolve_members(classes, decls=None, defines=None):
    """Turn ('member', name, decltype) into walker-executable tokens."""
    CONTAINER_ELEM = {
        "str": ("string",),
        "String": ("string",),
        "Vector": ("vec",),
        "int": ("int",),
        "float": ("float",),
    }
    decls = decls or {}

    # sizeof() on the 32-bit PPC target -- q_shared.h typedefs
    BASE_SIZE = {"int": 4, "unsigned": 4, "float": 4, "double": 8,
                 "qboolean": 4, "bool": 1, "char": 1, "byte": 1,
                 "short": 2, "unsignedshort": 2, "long": 4, "Vector": 12,
                 "Quat": 16, "vec3_t": 12, "vec4_t": 16, "str": 8}

    def sizeof(decltype, classes):
        t = re.sub(r"\s+", "", decltype or "")
        m = re.match(r"^(.*)\[([A-Za-z_]\w*)\]$", t)
        if m:
            base = sizeof(m.group(1), classes)
            n = defines.get(m.group(2))
            if base is None or n is None:
                return None
            return base * n
        m = re.match(r"^(.*)\[(\d+)\]$", t)
        if m:
            base = sizeof(m.group(1), classes)
            return None if base is None else base * int(m.group(2))
        t = t.rstrip("*")
        if t.endswith("*"):
            return 4
        return BASE_SIZE.get(t)

    def one(decltype, classes, depth=0):
        """-> token list for one declared member type, or None if unknown."""
        if decltype is None or depth > 6:
            return None
        t = re.sub(r"\s+", "", decltype)
        m = re.match(r"^Container<(.+)>$", t)
        if m:
            inner = m.group(1).rstrip("*")
            if inner in CONTAINER_ELEM:
                return [("container", None, CONTAINER_ELEM[inner][0])]
            if inner == "Class":          # archive.h:331 objptr+object
                return [("container", None, "objptr_object")]
            if inner == "Entity":         # archive.h:393 objptr only
                return [("container", None, "objptr")]
            if inner.endswith("Ptr"):     # archive.h:412 SafePtr<Entity>
                return [("container", None, "safeptr")]
            if inner in classes:
                return [("container", None,
                         resolve_toks(classes[inner][0], classes,
                                      depth + 1, {}))]
            return None
        m = re.match(r"^SafePtr<.+>$", t)
        if m:
            return [("safeptr", None, None)]
        if t in ("str", "String"):
            return [("string", None, None)]
        if t in classes:
            return resolve_toks(classes[t][0], classes, depth + 1, {})
        return None

    def resolve_toks(toks, classes, depth=0, mydecls=None):
        mydecls = mydecls or {}
        out = []
        for k, n, a in toks:
            if k == "member":
                sub = one(a, classes, depth)
                if sub is None:
                    return None
                out.extend(sub)
            elif k == "repeat":
                cnt, sub_toks = a
                r = resolve_toks(sub_toks, classes, depth + 1, mydecls)
                if r is None:
                    return None
                # `for ( i = 1; i <= num; i++ )` -- the count came off the
                # stream in the ArchiveInteger(&num) right before it, so drop
                # that int and let the walker read the count itself.
                if cnt[0] == "expr" and out and out[-1][0] == "int":
                    out.pop()
                    out.append(("repeat", n, (None, r)))
                else:
                    out.append(("repeat", n, (_const_bound(cnt, defines), r)))
            elif k == "rawsizeof":
                sz = (STRUCT_SIZES.get(n) or STRUCT_SIZES.get(a)
                      or sizeof(mydecls.get(n), classes))
                if sz is None:
                    return None
                out.append(("raw", n, sz))
            else:
                out.append((k, n, a))
        return out

    resolved = {}
    for name, (toks, src) in classes.items():
        r = resolve_toks(toks, classes, 0, decls.get(name, {}))
        if r is not None:
            resolved[name] = (r, src)
    return resolved


def _const_bound(cnt, defines=None):
    """A fixed C++ loop bound -> int, else None (read from the stream)."""
    if cnt[0] == "expr":
        b = cnt[1].strip()
        if re.match(r"^\d+$", b):
            return int(b)
        if defines and b in defines:
            return defines[b]
    return None


def _helper_fns(sources, defines):
    """Hand-written archive helpers that take ( Archiver &arc, ... ) and are
    called from inside a Class::Archive body -- G_ArchiveEdict in
    dlls/game/g_utils.cpp is the one the SDK ships.  Returned as
    {name: [tokens]} so parse_block can splice them."""
    out = {}
    pat = re.compile(r"\bvoid\s+([A-Za-z_]\w*)\s*\(\s*Archiver\s*&\s*arc\s*,")
    for src in sources.values():
        for m, body, _h in iter_matching(src, pat):
            name = m.group(1)
            if name in out:
                continue
            try:
                sub = []
                parse_block(body, {"decl": {}, "defines": defines,
                                   "helpers": out}, sub)
            except Unsupported:
                continue
            if sub:
                out[name] = sub
    return out


def _scan_defines(sources):
    """`#define NAME n` -> n, for the fixed-size table bounds the SDK loops on."""
    d = {}
    for src in sources.values():
        for m in re.finditer(r"^\s*#define\s+([A-Za-z_]\w*)\s+(\d+)\s*$",
                             src, re.M):
            d[m.group(1)] = int(m.group(2))
    return d


def summarise(tokens):
    """(kind, count) list -- human readable."""
    out = []
    for k, n, a in tokens:
        if k == "repeat":
            cnt, sub = a
            out.append("repeat(%s) x%s" % (summarise(sub), cnt[1]))
        elif k == "member":
            out.append("%s:%s" % (k, n))
        else:
            out.append(k + ("" if n is None else ":" + str(n)))
    return out


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else SDK_ROOT_CANDIDATES[0]
    classes, notes = extract(root)
    print("recovered %d class archive orders" % len(classes))
    for c in sorted(classes):
        toks, src = classes[c]
        print("\n== %s   [%s]  (%d tokens)" % (c, src, len(toks)))
        for line in summarise(toks):
            print("     ", line)
    if notes:
        print("\n%d classes skipped (unmodelled construct):" % len(notes))
        for nline in sorted(notes):
            print("   ", nline)


def load(sdk_root=None):
    """Public entry point used by sav_stream.py -> {classname: (tokens, cite)}"""
    root = sdk_root or SDK_ROOT_CANDIDATES[0]
    classes, notes = extract(root)
    sources = {}
    for p in collect_sdk_sources():
        try:
            t = open(p, encoding="latin1").read()
        except OSError:
            continue
        if "\r\n" in t:
            t = t.replace("\r\n", "\n")
        sources[p] = _strip_comments(t)
    defines = _scan_defines(sources)
    decls = _member_decls(sources)
    resolved = resolve_members(classes, decls, defines)
    return resolved, classes, notes


if __name__ == "__main__":
    main()
