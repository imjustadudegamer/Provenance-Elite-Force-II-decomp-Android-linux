#!/usr/bin/env python3
"""
EF2 guard-diff sweep.

For every function that exists in BOTH the port fgame and the retail SDK,
compare the normalized set of control-flow guard conditions (`if`/`while`/
`for` heads).  Retail-only conditions that the port dropped are the class of
bug that hung the map (Archiver::Close's `if (file)` guard was missing, so an
unopened Archiver hit ArchiveFile::Write's 0-doubling loop).

Not a full transliteration diff -- it finds *candidate* divergences to verify
by hand.  Low signal on functions that were deliberately port-adapted; check
each hit against the SDK + Mac decompile before touching it.
"""
import os, re, sys

PORT_DIR = "openmohaa/code/fgame"
RET_DIR  = "docs/evidence/ef2-sdk/ef2-sdk/dlls/game"

def read(fn):
    with open(fn, encoding="latin1") as f:
        return f.read()

def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    src = re.sub(r"//[^\n]*", " ", src)
    return src

def extract_functions(src):
    """Return {name: body} for top-level `TYPE Class::name(` or `TYPE name(`."""
    src = strip_comments(src)
    fns = {}
    i = 0
    n = len(src)
    pat = re.compile(r"([A-Za-z_][\w:<>*& ]*?)\s+((?:[A-Za-z_]\w*::)+[A-Za-z_]\w*)\s*\(([^;]*?)\)\s*\{", re.S)
    for m in pat.finditer(src):
        name = m.group(2)
        start = m.end() - 1  # the '{'
        depth = 0
        j = start
        while j < n:
            c = src[j]
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    break
            j += 1
        if depth == 0:
            fns.setdefault(name, []).append(src[start:j+1])
        # resume after the brace to avoid re-matching inside
    return fns

def norm_id(s):
    """Normalize port-vs-retail naming idioms that are NOT behavioural:
    m_ member prefix, q_stricmp/Q_stricmp vs stricmp, ai_ getter prefixes."""
    s = re.sub(r"\bm_", "", s)                    # m_behavior -> behavior
    s = re.sub(r"\bq_stricmpn\b", "stricmpn", s)
    s = re.sub(r"\bQ_stricmpn\b", "stricmpn", s)
    s = re.sub(r"\bq_stricmp\b", "stricmp", s)
    s = re.sub(r"\bQ_stricmp\b", "stricmp", s)
    s = re.sub(r"\bq_true\b", "true", s)
    s = re.sub(r"\bq_false\b", "false", s)
    s = re.sub(r"\bqboolean\b", "bool", s)
    s = re.sub(r"\bAI_Get", "", s)                # ai_getenemy -> enemy
    return s

def normalize(body):
    """Lowercase + strip spaces so `if ( file )` == `if(file)`. Keep identifiers."""
    return norm_id(re.sub(r"\s+", "", body).lower())

def guard_conditions(body):
    """Return a normalized multiset of control-flow heads: `if(expr)`, `while(..)`, `for(..)`."""
    heads = []
    body = strip_comments(body)
    # if/while: keyword then balanced parens (no brace)
    for kw in ("if", "while"):
        for m in re.finditer(r"\b%s\s*\((.*?)\)" % kw, body, re.S):
            cond = norm_id(re.sub(r"\s+", "", m.group(1)).lower())
            heads.append(f"{kw}({cond})")
    # for: normalize the loop variable declaration away (`for(int i=1;...` == `for(i=1;...`)
    for m in re.finditer(r"\bfor\s*\((.*?)\)", body, re.S):
        cond = re.sub(r"\s+", "", m.group(1)).lower()
        cond = re.sub(r"^int", "", cond)          # for(inti= -> for(i=
        cond = norm_id(cond)
        heads.append(f"for({cond})")
    return heads

def diff_guards(port_fn, ret_fn, name):
    """Report conditions in retail that port lacks, and vice versa."""
    pg = set(guard_conditions(port_fn))
    rg = set(guard_conditions(ret_fn))
    missing = rg - pg   # retail has, port dropped
    extra   = pg - rg   # port added
    return missing, extra

def load_mac_names():
    """Map demangled name -> mangled name from game_dll_symbols.txt (Mac PPC build)."""
    mapping = {}
    try:
        with open("docs/evidence/mac_symbols/game_dll_symbols.txt", encoding="latin1") as f:
            for line in f:
                parts = line.split()
                if len(parts) >= 3 and "." in parts[1]:
                    mangled = parts[1].lstrip(".")
                    demangled = parts[2]
                    # strip `(args)` and ` const` so Actor::EndMode(void) -> Actor::EndMode
                    base = demangled.split("(", 1)[0].rstrip()
                    mapping[base] = mangled
    except FileNotFoundError:
        pass
    return mapping

def main():
    mac = load_mac_names()
    if len(sys.argv) > 1:
        names = set(sys.argv[1:])
    else:
        names = None

    port_all = {}
    for f in sorted(os.listdir(PORT_DIR)):
        if f.endswith(".cpp"):
            for name, bodies in extract_functions(read(os.path.join(PORT_DIR, f))).items():
                port_all.setdefault(name, []).extend(bodies)

    ret_all = {}
    for f in sorted(os.listdir(RET_DIR)):
        if f.endswith(".cpp"):
            for name, bodies in extract_functions(read(os.path.join(RET_DIR, f))).items():
                ret_all.setdefault(name, []).extend(bodies)

    shared = sorted(set(port_all) & set(ret_all))
    print(f"shared functions: {len(shared)}")
    print(f"{'FUNCTION':<50} {'RETAIL-ONLY GUARDS (port lost)':<60} {'Mac mangled name'}")
    # Only report functions where RETAIL has a guard the port lacks (the crash-class
    # bug); port-only additions are null-guards/adaptations and are logged separately.
    hits = []
    for name in shared:
        if names and not any(name.startswith(n) or n in name for n in names):
            continue
        for pb in port_all[name][:1]:
            for rb in ret_all[name][:1]:
                missing, extra = diff_guards(pb, rb, name)
                if missing:
                    hits.append((name, missing, extra))
    for name, missing, extra in hits:
        ms = " | ".join(sorted(missing))[:58]
        macname = mac.get(name, "?")
        print(f"{name[:49]:<50} {ms:<60} {macname}")
    print(f"\n{len(hits)} functions have retail-only guards to verify.")

if __name__ == "__main__":
    main()
