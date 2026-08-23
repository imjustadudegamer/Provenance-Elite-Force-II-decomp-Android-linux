#!/usr/bin/env python3
"""Port-only class-member census -- the mechanical half of MOHAA_RESIDUE_PLAN 4.3.

For each class whose header exists in BOTH the retail EF2 SDK
(personal/evidence/ef2-sdk/ef2-sdk/dlls/game/*.h) and the port (code/fgame/*.h):

  1. parse the SDK header  -> the authoritative member set of that class
  2. parse the port header -> the port's member set
  3. the difference is the port-only candidate list
  4. for each candidate, count EVERY textual occurrence of its name across
     code/{fgame,cgame,client,qcommon,server,script,corepp} -- declarations,
     constructor initialiser lists, comments, string literals, everything.  A
     count of 1 means the declaration is the only reference in the tree and the
     member is provably dead; 2 usually means declaration + ctor init.

Deliberate traps handled (MOHAA_RESIDUE_PLAN 0):
  * SDK headers are CRLF            -> tr -d '\\r'
  * `inline void X::Archive(...)`   -> not a member declaration
  * CLASS_DECLARATION lives in the .cpp and takes a 3rd string arg, so
    brace-tracking a class must not be fooled by the .cpp
  * member names are matched on word boundaries so m_Anim does not hit m_AnimFoo

Output: a table ready to feed the deletion pass, plus
/tmp/ef2docs/members.json for the deleter.
"""
import os
import json, os, re, sys

SDK = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + "/personal/evidence/ef2-sdk/ef2-sdk/dlls/game"
PORT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + "/app/src/main/cpp/code/fgame"
SCAN = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))) + "/app/src/main/cpp/code"

CLASSES = ["actor", "animate", "camera", "entity", "equipment", "item", "level",
           "player", "playerstart", "scriptmaster", "scriptslave", "sentient",
           "trigger", "weapon"]

BLOB = None
IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")

# A member declaration starts a line inside the class body with a type and a
# name, and does not start with these.
NOT_DECL = re.compile(
    r"^\s*(//|/\*|\*|#|typedef|using|class\s|struct\s|enum\s|template|friend|"
    r"public:|private:|protected:|\{|\}|virtual\s+~|~)")


def strip_comments_and_strings(text):
    out = []
    i, n = 0, len(text)
    in_block = in_line = in_str = in_chr = False
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if in_block:
            if c == "*" and nxt == "/":
                in_block = False; i += 2; out.append(" ")
            else:
                out.append("\n" if c == "\n" else " "); i += 1
        elif in_line:
            if c == "\n":
                in_line = False; out.append("\n")
            else:
                out.append(" ")
            i += 1
        elif in_str:
            if c == "\\": out.append(" "); i += 2
            elif c == '"': in_str = False; out.append(" "); i += 1
            else: out.append(" "); i += 1
        elif in_chr:
            if c == "\\": out.append(" "); i += 2
            elif c == "'": in_chr = False; out.append(" "); i += 1
            else: out.append(" "); i += 1
        else:
            if c == "/" and nxt == "*": in_block = True; i += 2; out.append(" ")
            elif c == "/" and nxt == "/": in_line = True; i += 2; out.append(" ")
            elif c == '"': in_str = True; i += 1; out.append(" ")
            elif c == "'": in_chr = True; i += 1; out.append(" ")
            else: out.append(c); i += 1
    return "".join(out)


def read(path):
    return strip_comments_and_strings(open(path, encoding="latin1").read().replace("\r", ""))


def class_body(text, classname):
    """Body of `class classname [: ...] { ... };` -- skips forward declarations."""
    for m in re.finditer(r"\bclass\s+%s\b" % re.escape(classname), text):
        brace = text.find("{", m.end())
        semi = text.find(";", m.end())
        if brace < 0:
            continue
        if 0 <= semi < brace:
            continue            # `class Foo;` forward declaration
        if brace - m.end() > 400:
            continue            # a different Foo far away
        i = brace
        depth, j = 1, i + 1
        while j < len(text) and depth:
            if text[j] == "{": depth += 1
            elif text[j] == "}": depth -= 1
            j += 1
        return text[i:j]
    return None


def members(body):
    """Names declared as data members.  Skips function definitions by requiring
    no '(' before the ';' and no '::' scope."""
    names = []
    for line in body.split("\n"):
        if NOT_DECL.match(line):
            continue
        line = line.strip()
        if not line or "(" in line or ")" in line:
            continue
        if "{" in line or "}" in line or "::" in line:
            continue
        # strip the trailing initialiser / array / bits
        core = line.rstrip(";").strip()
        toks = IDENT.findall(core)
        if not toks:
            continue
        name = toks[-1]
        if len(name) < 2:
            continue
        names.append(name)
    return set(names)


def all_class_members(path):
    text = read(path)
    acc = {}
    for m in re.finditer(r"\bclass\s+([A-Za-z_][A-Za-z0-9_]*)", text):
        cname = m.group(1)
        body = class_body(text, cname)
        if body:
            acc[cname] = members(body)
    return acc


def main():
    result = {}
    total = 0
    for cls in CLASSES:
        sp = os.path.join(SDK, cls + ".h")
        pp = os.path.join(PORT, cls + ".h")
        if cls == "playerstart":
            pp = os.path.join(PORT, "playerstart.h")
        if not (os.path.exists(sp) and os.path.exists(pp)):
            print("SKIP %s (sdk %s / port %s)" % (cls, os.path.exists(sp), os.path.exists(pp)))
            continue
        sdk = all_class_members(sp)
        port = all_class_members(pp)
        sdk_names = set()
        for v in sdk.values():
            sdk_names |= v
        port_names = set()
        for v in port.values():
            port_names |= v
        port_only = sorted(port_names - sdk_names)
        if not port_only:
            continue
        # occurrence count across the tree (tree read once, cached)
        global BLOB
        if BLOB is None:
            BLOB = []
            for root, dirs, files in os.walk(SCAN):
                dirs[:] = [d for d in dirs if d not in ("tools", "botlib", "gamespy", "renderergl2")]
                for f in files:
                    if f.endswith((".c", ".cpp", ".h", ".hpp")):
                        try:
                            BLOB.append(open(os.path.join(root, f), encoding="latin1",
                                             errors="replace").read())
                        except Exception:
                            pass
        counts = {}
        for name in port_only:
            n = 0
            pat = re.compile(r"\b%s\b" % re.escape(name))
            for txt in BLOB:
                n += len(pat.findall(txt))
            counts[name] = n
        result[cls] = {
            "port_classes": sorted(port),
            "sdk_members": len(sdk_names),
            "port_members": len(port_names),
            "port_only": [{"name": k, "refs": counts[k]} for k in port_only],
        }
        dead = [k for k in port_only if counts[k] <= 2]
        total += len(port_only)
        print("%-14s sdk=%4d port=%4d port-only=%4d  (<=2 refs: %3d, 1-2 refs: %3d)"
              % (cls, len(sdk_names), len(port_names), len(port_only),
                 len([k for k in port_only if counts[k] == 1]), len(dead)))

    print("\nTOTAL port-only members: %d" % total)
    json.dump(result, open("/tmp/ef2docs/members.json", "w"), indent=1)


if __name__ == "__main__":
    main()
