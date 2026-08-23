#!/usr/bin/env python3
"""ef2_method_census.py -- port-only METHOD census for one fgame class.

member_census.py only sees FIELDS; port-only methods are invisible to it.
This tool extracts the declared method surface of one class out of a port
header, extracts the same surface out of the retail SDK (headers UNION every
`cls::name(` occurrence in the SDK tree), and reports

  candidates = port_methods(class) - sdk_methods(class)   (no SDK declaration)

plus, for every candidate, whether the retail Mac GAME binary still carries a
symbol for it (mangled and demangled columns of game_dll_symbols.txt, and the
decomp .c file names, which are mangled).

--nodefs additionally lists declared methods with no definition and no
reference anywhere in the port tree (a declaration of a retail method must
NEVER be deleted for that reason -- see the 0905 verdicts below).

VERDICT TABLE, TASK 7 (Player/Weapon/Level/Entity/Sentient method tranches),
port @ 98db44d:

  Player    port 507 / SDK 519.  4 port-only, ALL LIVE, none deletable:
            FindObjectiveIndex    <- retail Player::SetObjectiveShow/
                                     SetObjectiveComplete/SetObjectiveFailed
            GetAffectingViewModes <- retail Player::ShouldSendToClient
                                     (SDK player.h:952, player.cpp:11908)
            LoadObjectiveSet      <- retail Player::loadObjectives
            ResetClient           <- retail G_ClientBegin (call site is an OPM
                                     addition; DEFINITION LIVES IN player.cpp)
  Entity    port 277 / SDK 272.  6 port-only, ALL LIVE (animate/mover component
            plumbing): ClearAnimSlot + EF2AnimChannelForSlot <- animate.cpp;
            EF2_AnimateComponentPresent/EF2_MoverComponentPresent/
            EF2_CreateAnimateComponent/EF2_CreateMoverComponent <- entity.cpp,
            object.cpp, sentient.cpp, item.cpp, viewthing.cpp, body.cpp,
            dispenser.cpp, scriptslave.cpp.
  Weapon    port 235 / SDK 235.  0 port-only.  GetMaxAmmo/SetModels are
            declared but undefined -- RETAIL DOES THE SAME (SDK weapon.h:432,
            :440 define neither), so the port is verbatim; keep.
  Level     port 34  / SDK 34.   0 port-only, 0 dead.
  Sentient  port 137 / SDK 138.  0 port-only.  BestWeapon/NextWeapon/
            PreviousWeapon/NumInventoryItems are declared but not defined in
            the port -- retail DEFINES all four (SDK sentient.cpp:1526, :1556,
            :1618, :3046): missing implementations, do not delete.
  Player    NODEF GetFov (SDK player.h:870 inline), Taunt (player.h:764),
            UpdateViewAngles (player.h:668): retail, missing implementations.
  Also missing from the port, retail Player: ClearPoints, DonePutaway,
  GetDamageMultiplier, GetNumPoints, GetTargetEnemyLocked, IsNewActiveWeapon,
  SetCurrentCombo, SetIncomingMeleeAttack, SetPowerupTimer, StepUp,
  UpdatePowerupTimer, checkcanwallhug, checkfakeplayerholster, checkhangatwall,
  checkholstercomplete, checkuseweaponleft; retail Entity: setGameplayDamage
  (SDK entity.h:862).

Net deletions for TASK 7: ZERO.  Every port-only method in these five classes
is reached from retail-present code.

usage: ef2_method_census.py [--port DIR] [--sdk DIR] [--sym FILE] CLASS HEADER
"""
import argparse
import os
import re
import subprocess
import sys

KEYWORDS = {
    "if", "for", "while", "switch", "sizeof", "return", "defined", "alignof",
    "decltype", "static_assert", "emit", "class", "struct", "enum", "union",
    "override", "final", "const", "static", "inline", "explicit", "virtual",
    "friend", "operator", "template", "using", "namespace", "sizeof...",
}
IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
# identifier immediately followed by '('
CALL = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(")


def ident_before(ln, i):
    """The identifier immediately preceding position i (skipping blanks)."""
    j = i - 1
    while j >= 0 and ln[j] in " \t":
        j -= 1
    k = j
    while k >= 0 and (ln[k].isalnum() or ln[k] == "_"):
        k -= 1
    name = ln[k + 1:j + 1]
    return name if IDENT.match(name) else None


def class_body(path, cls):
    """Return (start_line, end_line) 1-based inclusive of class cls's body."""
    with open(path, "r", errors="replace") as fh:
        lines = fh.readlines()
    head = re.compile(r"^\s*(?:template\s*<[^>]*>\s*)?class\s+" + re.escape(cls) + r"\b")
    start = None
    for i, ln in enumerate(lines):
        if head.match(ln) and "//" not in ln.split(cls, 1)[1][:1]:
            start = i
            break
    if start is None:
        return None
    # find the opening brace of this class, skipping any base-class clause
    depth = 0
    body_start = None
    for i in range(start, len(lines)):
        for ch in lines[i]:
            if ch == "{":
                depth += 1
                if depth == 1:
                    body_start = i
            elif ch == "}":
                depth -= 1
                if depth == 0 and body_start is not None:
                    return (body_start + 1, i + 1)
    return None


def strip_comments_strings(text):
    """Blank out comments and string/char literals, PRESERVING newlines and
    column positions so reported line numbers match the original file."""
    out = list(text)
    i = 0
    n = len(text)

    def blank(a, b):
        for j in range(a, min(b, n)):
            if out[j] != "\n":
                out[j] = " "

    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = i
            while j < n and text[j] != "\n":
                j += 1
            blank(i, j)
            i = j
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            j = i + 2
            while j + 1 < n and not (text[j] == "*" and text[j + 1] == "/"):
                j += 1
            blank(i, j + 2)
            i = j + 2
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                if text[j] == "\\":
                    j += 1
                j += 1
            blank(i, j + 1)
            i = j + 1
        else:
            i += 1
    return "".join(out)


def methods_in_class(path, cls):
    """Yield (name, line) for every member declared in class cls of file path.

    Only declarations whose '(' is entered while the class-body depth is 0
    count, so nested-class members and default-argument calls are excluded.
    """
    with open(path, "r", errors="replace") as fh:
        raw = fh.read()
    text = strip_comments_strings(raw)
    lines = text.split("\n")
    body = class_body_lines(text, cls, path)
    if body is None:
        return []
    open_idx, close_idx = body
    res = []
    depth = 0  # scanning starts on the '{' line, so members live at depth 1
    tmpl = 0
    for idx in range(open_idx, close_idx):
        ln = lines[idx]
        i = 0
        n = len(ln)
        while i < n:
            ch = ln[i]
            if ch == "<" and depth == 1:
                tmpl += 1
            elif ch == ">" and tmpl > 0 and depth == 1:
                tmpl -= 1
            elif ch == "{" or ch == "(":
                # record the member name when the '(' opens at member depth
                if ch == "(" and depth == 1 and tmpl == 0:
                    name = ident_before(ln, i)
                    if name and name not in KEYWORDS:
                        res.append((name, idx + 1))
                depth += 1
            elif ch == "}" or ch == ")":
                depth -= 1
            i += 1
    return res


def class_body_lines(text, cls, path):
    """Return (open_idx, close_idx), 0-based indices of the line carrying the
    class's own '{' and the line carrying its matching '}'.

    A forward declaration (`class X;`) is not a definition and is skipped, so
    the first `class X` line whose remainder holds no ';' is used."""
    lines = text.split("\n")
    head = re.compile(r"^\s*(?:template\s*<[^>]*>\s*)?class\s+" + re.escape(cls) + r"\b")
    start = None
    for i, ln in enumerate(lines):
        if head.match(ln) and ";" not in ln[ln.index(cls) + len(cls):]:
            start = i
            break
    if start is None:
        return None
    depth = 0
    open_idx = None
    for i in range(start, len(lines)):
        for ch in lines[i]:
            if ch == "{":
                depth += 1
                if depth == 1 and open_idx is None:
                    open_idx = i
            elif ch == "}":
                depth -= 1
                if depth == 0 and open_idx is not None:
                    return (open_idx, i)
    return None


def sdk_methods(sdk_dir, cls, sdk_root=None):
    """Retail SDK method surface for cls:

      1. every method declared in an SDK header class body, UNION
      2. every `cls::name(` occurrence anywhere in the SDK tree.

    (2) is mandatory: the shipped SDK headers are INCOMPLETE -- e.g.
    Player::ShouldSendToClient is defined at SDK player.cpp:11908 but is
    declared nowhere in SDK player.h.  Judging against headers only produces
    false port-only candidates.
    """
    out = {}
    for root, _dirs, files in os.walk(sdk_dir):
        for f in files:
            if not f.endswith(".h"):
                continue
            p = os.path.join(root, f)
            try:
                for name, ln in methods_in_class(p, cls):
                    out.setdefault(name, []).append(f + ":" + str(ln))
            except Exception as exc:  # noqa: BLE001
                sys.stderr.write("warn: %s: %s\n" % (p, exc))
    if sdk_root is None:
        sdk_root = os.path.dirname(os.path.abspath(sdk_dir))
    pat = re.compile(r"\b" + re.escape(cls) + r"\s*::\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(")
    for root, _dirs, files in os.walk(sdk_root):
        for f in files:
            if not f.endswith((".cpp", ".h", ".hpp")):
                continue
            p = os.path.join(root, f)
            try:
                text = strip_comments_strings(open(p, errors="replace").read())
            except OSError:
                continue
            for m in pat.finditer(text):
                name = m.group(1)
                line = text.count("\n", 0, m.start()) + 1
                loc = os.path.relpath(p, sdk_root) + ":" + str(line)
                if loc not in out.setdefault(name, []):
                    out[name].append(loc)
    return out


def port_methods(port_dir, cls, header):
    """Declared methods in the port header, plus anywhere else in code/fgame."""
    out = {}
    p = os.path.join(port_dir, header)
    for name, ln in methods_in_class(p, cls):
        out.setdefault(name, []).append(os.path.basename(header) + ":" + str(ln))
    return out


def retail_symbol_hits(sym_file, decomp_dir, cls, name):
    """True if the retail GAME binary carries any symbol for cls::name."""
    hits = []
    dem = "%s::%s(" % (cls, name)
    try:
        with open(sym_file, "r", errors="replace") as fh:
            for ln in fh:
                if dem in ln:
                    hits.append(ln.rstrip())
    except OSError:
        pass
    # mangled: name__<len>cls  (file-name prefix in the decomp dir)
    pat = re.compile(r"^\.?" + re.escape(name) + r"__" + mangled_class(cls) + r"\b")
    for root, _d, files in os.walk(decomp_dir):
        for f in files:
            if pat.match(f):
                hits.append("decomp:" + os.path.join(root, f))
    return hits


def mangled_class(cls):
    """Class name as it appears in the HP/cfront mangling used by the retail
    PPC build, e.g. Player -> 6Player, SafePtr<8Sentient> handled by caller."""
    inner = re.sub(r"[^A-Za-z0-9_]", "", cls)
    return str(len(inner)) + inner


def tree_refs(port_code_dir, name):
    """Word-boundary reference count for name over the port code tree."""
    try:
        p = subprocess.run(
            ["/bin/grep", "-rnE", r"\b" + name + r"\b", port_code_dir],
            capture_output=True, text=True, check=False)
    except OSError:
        return []
    return [l for l in p.stdout.split("\n") if l]


_CORPUS = {}


def corpus(port_code_dir):
    """(path, comment-stripped text) for every C++ file under the port tree,
    loaded once per process."""
    if not _CORPUS:
        for root, _dirs, files in os.walk(port_code_dir):
            for f in files:
                if not f.endswith((".cpp", ".h", ".hpp", ".cc")):
                    continue
                p = os.path.join(root, f)
                try:
                    _CORPUS[p] = strip_comments_strings(open(p, errors="replace").read())
                except OSError:
                    pass
    return _CORPUS


def missing_defs(port_code_dir, cls, names, header_path):
    """Declared methods of cls with no `cls::name(` occurrence anywhere in the
    port tree and no inline body in the header (single corpus pass)."""
    pat = re.compile(r"\b" + re.escape(cls) + r"\s*::\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(")
    found = set()
    for _p, text in corpus(port_code_dir).items():
        for m in pat.finditer(text):
            found.add(m.group(1))
    hdr = open(header_path, "r", errors="replace").read()
    for n in names:
        if re.search(r"\b" + re.escape(n) + r"\s*\([^;]*\)\s*(?:const\s*)?\{", hdr):
            found.add(n)
    return sorted(set(names) - found)


def undeclared_defs(port_code_dir, cls, port_dir, header):
    """Class member definitions in the port with no declaration in the port
    header (these are invisible to a header-only census)."""
    decls = set(n for n, _ in methods_in_class(os.path.join(port_dir, header), cls))
    try:
        p = subprocess.run(
            ["/bin/grep", "-rnE", r"^(static\s+)?[A-Za-z_][A-Za-z0-9_:<>\*& ]*[~]?" +
             re.escape(cls) + r"::[A-Za-z_][A-Za-z0-9_]*\s*\(",
             port_code_dir],
            capture_output=True, text=True, check=False)
    except OSError:
        return []
    found = set()
    for l in p.stdout.split("\n"):
        if not l or "//" in l.split("::")[0]:
            continue
        m = re.search(re.escape(cls) + r"::([A-Za-z_][A-Za-z0-9_]*)\s*\(", l)
        if m:
            found.add(m.group(1))
    return sorted(found - decls)


def default_port_dir():
    """code/fgame of the repo this script lives in (tools/ef2/<here>)."""
    here = os.path.dirname(os.path.abspath(__file__))
    for cand in (os.path.join(here, "..", "..", "code", "fgame"),
                 os.path.join(here, "..", "code", "fgame"),
                 os.path.join(os.getcwd(), "code", "fgame")):
        p = os.path.abspath(cand)
        if os.path.isdir(p):
            return p
    return os.path.abspath("code/fgame")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cls")
    ap.add_argument("header")
    ap.add_argument("--port", default=None)
    ap.add_argument("--sdk", default=os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/personal/evidence/ef2-sdk/ef2-sdk/dlls/game")
    ap.add_argument("--sym", default=os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/personal/evidence/mac_symbols/game_dll_symbols.txt")
    ap.add_argument("--decomp", default=os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))) + "/personal/evidence/mac_symbols/game_decomp")
    ap.add_argument("--refs", action="store_true", help="also show port-tree reference counts")
    ap.add_argument("--nodefs", action="store_true", help="report declarations with no definition")
    ap.add_argument("--undeclared", action="store_true",
                    help="report port definitions absent from the port header")
    a = ap.parse_args()
    port_dir = a.port
    if port_dir is None:
        port_dir = default_port_dir()
    pm = port_methods(port_dir, a.cls, a.header)
    sm = sdk_methods(a.sdk, a.cls)
    cand = sorted(set(pm) - set(sm))
    print("== %s (%s)" % (a.cls, a.header))
    print("port methods: %d   sdk methods: %d   candidates(port-only): %d"
          % (len(pm), len(sm), len(cand)))
    for name in cand:
        sym = retail_symbol_hits(a.sym, a.decomp, a.cls, name)
        tag = "RETAIL-SYMBOL" if sym else "no-retail-symbol"
        print("  %-45s port@%s  %s" % (name, ",".join(pm[name])[:28], tag))
        for h in sym[:3]:
            print("        %s" % h[:170])
        if a.refs:
            refs = tree_refs(port_dir, name)
            print("        refs(%d):" % len(refs))
            for r in refs[:12]:
                print("          %s" % r[:170])
    if a.nodefs or a.undeclared:
        code_dir = os.path.dirname(port_dir)
        for name in missing_defs(code_dir, a.cls, sorted(set(pm)),
                                 os.path.join(port_dir, a.header)):
            print("NODEF %-45s declared %s" % (name, pm[name]))
    if a.undeclared:
        code_dir = os.path.dirname(port_dir)
        for name in undeclared_defs(code_dir, a.cls, port_dir, a.header):
            print("UNDECLARED-DEF %s::%s" % (a.cls, name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
