#!/usr/bin/env python3
"""Offline EF2 .tik surface/skin census -- no game launch.

Runs the port's .tik shader-binding rules (code/tiki/tiki_parse.cpp,
TIKI_ParseSetup's `surface <name> shader <token>` arm) against every shipped
.tik, then checks each computed shader name against the retail image and
shader-script universe.  Anything that resolves to nothing is what renders
with the renderer's default (white) shader -- R_RegisterShaders stores
hShader = 0 when the surface's shader-name slot is empty or its image is
missing, and R_AddSkelSurfaces draws tr.shaders[0] unconditionally.

Usage:
  python3 tools/ef2/tik_surface_census.py [--base DIR] [--pk3 GLOB]...
        [--json OUT] [--verbose] [--max N]

With no --base the retail install is used read-only.
"""

import argparse
import fnmatch
import json
import os
import posixpath
import re
import sys
import zipfile

DEFAULT_BASE = os.path.expanduser("~/Star Trek Elite Force II/base")


# --------------------------------------------------------------------------
# tokenizing -- mirrors the port's dloaddef_t::GetToken on a .tik text file:
# whitespace separated, // to end of line, /* */ block comments, and "quoted
# strings" kept whole.  Newlines are tracked so `crossLine` semantics survive.
# --------------------------------------------------------------------------
class Tokenizer:
    def __init__(self, text):
        self.toks = []
        self._lex(text)
        self.i = 0

    def _lex(self, text):
        i, n = 0, len(text)
        while i < n:
            c = text[i]
            if c in " \t\r\n":
                i += 1
                continue
            if c == "/" and i + 1 < n and text[i + 1] == "/":
                while i < n and text[i] != "\n":
                    i += 1
                continue
            if c == "/" and i + 1 < n and text[i + 1] == "*":
                j = text.find("*/", i + 2)
                i = n if j < 0 else j + 2
                continue
            if c == '"':
                j = i + 1
                buf = []
                while j < n and text[j] != '"':
                    buf.append(text[j])
                    j += 1
                self.toks.append(("".join(buf)).strip() or '""')
                i = j + 1
                continue
            j = i
            while j < n and text[j] not in " \t\r\n":
                j += 1
            self.toks.append(text[i:j])
            i = j

    def next(self):
        t = self.toks[self.i] if self.i < len(self.toks) else None
        if t is not None:
            self.i += 1
        return t

    def peek(self):
        return self.toks[self.i] if self.i < len(self.toks) else None


class PakSet:
    """Read-only view over every retail .pk3, plus any loose files on disk."""

    def __init__(self, base):
        self.base = base
        self.zf = {}
        self.names = {}          # lowercased path -> owning pk3
        self.loose = set()       # lowercased relative paths present on disk
        for fn in sorted(os.listdir(base)):
            if fn.lower().endswith(".pk3"):
                try:
                    z = zipfile.ZipFile(os.path.join(base, fn))
                except Exception as e:  # pragma: no cover
                    sys.stderr.write("warn: %s: %s\n" % (fn, e))
                    continue
                self.zf[fn] = z
                for info in z.infolist():
                    self.names[info.filename.replace("\\", "/").lower()] = fn
        for root, _dirs, files in os.walk(base):
            for f in files:
                rel = os.path.relpath(os.path.join(root, f), base)
                self.loose.add(rel.replace("\\", "/").lower())

    def has(self, path):
        p = path.replace("\\", "/").lower()
        return p in self.names or p in self.loose

    def read(self, path):
        p = path.replace("\\", "/").lower()
        if p in self.names:
            return self.zf[self.names[p]].read(p)
        fp = os.path.join(self.base, *p.split("/"))
        with open(fp, "rb") as fh:
            return fh.read()

    def exists(self, path):
        return self.has(path)

    def glob(self, pattern):
        out = []
        for n in list(self.names) + list(self.loose):
            if fnmatch.fnmatch(n, pattern):
                out.append(n)
        return sorted(out)


# --------------------------------------------------------------------------
# the .tik walk
# --------------------------------------------------------------------------

class TikParser:
    """Transcription of the port's setup-block shader binding.

    Only the commands that decide surface/skin state are modelled; everything
    else (lod, radius, headoffset, bonemapping, expressions, anims) is skipped
    exactly as far as surface state is concerned.  `init` blocks are walked for
    `surface <name> <flags>` because those are the *entity-state* nodraw/skin
    bits and are what the comm-port [HP] rows were printing.
    """

    def __init__(self, paks, maxdepth=12):
        self.paks = paks
        self.maxdepth = maxdepth
        self.cache = {}
        self.missing_includes = set()

    def load(self, path, depth=0, stack=()):
        key = path.lower()
        if key in self.cache:
            return self.cache[key]
        if depth > self.maxdepth or key in stack:
            return None
        if not self.paks.exists(path):
            self.missing_includes.add(path)
            return None
        try:
            text = self.paks.read(path).decode("latin-1")
        except Exception:
            self.missing_includes.add(path)
            return None

        st = {
            "path": posixpath.dirname(path) + "/",
            "texturepath": "",
            "surfaces": {},     # name -> {"shaders": [...], "flags": int}
            "order": [],
            "includes": [],
            "init_server_surfaces": [],   # (name, flagword)
        }
        self.cache[key] = st
        tok = Tokenizer(text)
        self._parse_tik(tok, st, path, depth, stack + (key,))
        return st

    # -- one .tik file ----------------------------------------------------
    def _parse_tik(self, tok, st, path, depth, stack):
        texturepath = ""          # per-script, does not leak across includes
        cur_surf = None

        while True:
            t = tok.next()
            if t is None:
                break
            low = t.lower()

            if low == "$include":
                inc = tok.next()
                if inc:
                    self.load(inc, depth + 1, stack)
                continue
            if low == "setup":
                # `path` is the *including* script's dir for nested scripts;
                # the port keeps currentScript->path per script file.
                self._parse_block(tok, st, path, texturepath_ref=None,
                                  setup=True, state={"texturepath": texturepath})
                texturepath = st.get("_last_texturepath", texturepath)
                continue
            if low == "init":
                self._parse_init(tok, st)
                continue
            # anything else at top level (client/server/expressions, anim names)
            continue

    def _parse_block(self, tok, st, path, texturepath_ref, setup, state):
        texturepath = ""
        cur = None
        while True:
            t = tok.next()
            if t is None:
                return
            low = t.lower()
            if low == "{":
                continue
            if low == "}":
                st["_last_texturepath"] = texturepath
                return
            if not setup:
                continue
            if low == "texturepath":
                v = tok.next() or ""
                texturepath = v.replace("\\", "/")
                if texturepath and not texturepath.endswith("/"):
                    texturepath += "/"
                st["_last_texturepath"] = texturepath
                continue
            if low == "path":
                # tiki_parse.cpp:1244 -- `path` REWRITES currentScript->path, it
                # does not just record it.  Every later `surface ... shader
                # <ext-bearing token>` that has no texturepath and no separator
                # resolves against this, so ignoring it sends the name to the
                # wrong directory (starfleet_tuvok-training's `path
                # models/char/starfleet/tuvok/` + `tuvok-head.tga`).
                v = (tok.next() or "").replace("\\", "/")
                st["path"] = v if v.endswith("/") else v + "/"
                continue
            if low == "surface":
                name = tok.next()
                if not name:
                    continue
                ent = st["surfaces"].setdefault(name, {"shaders": [], "flags": 0})
                if name not in st["order"]:
                    st["order"].append(name)
                cur = ent
                # `surface <name> shader <tok> [flags <word>]`
                sub = tok.next()
                while sub is not None and sub.lower() not in (
                    "shader", "flags", "damage", "nodraw", "}"):
                    if tok.peek() is None or tok.peek().lower() in (
                        "surface", "skelmodel", "morphfile", "path",
                        "texturepath", "lod", "$include", "init", "}"):
                        break
                    sub = tok.next()
                if sub is None:
                    continue
                if sub.lower() == "shader":
                    shtok = tok.next()
                    if shtok:
                        ent["shaders"].append(self._shader_name(shtok, st, texturepath))
                    # optional trailing `flags <x>`
                    if (tok.peek() or "").lower() == "flags":
                        tok.next()
                        ent["flags"] |= self._flagword(tok.next() or "")
                elif sub.lower() == "flags":
                    ent["flags"] |= self._flagword(tok.next() or "")
                continue
            # skip this command's tokens: a command ends at the next token that
            # is itself a known command or a brace -- .tik commands are 0..5 args
            n = 0
            while tok.peek() is not None and tok.peek().lower() not in _CMD_WORDS and n < 6:
                tok.next()
                n += 1
        # unreachable

    def _shader_name(self, shtok, st, texturepath):
        """code/tiki/tiki_parse.cpp: the three-branch shader-name rule."""
        has_dot = "." in shtok
        has_sep = ("/" in shtok) or ("\\" in shtok)
        if texturepath and has_dot and not has_sep:
            return texturepath + shtok
        if has_dot:
            return st["path"] + shtok
        return shtok

    def _flagword(self, w):
        w = (w or "").lower().lstrip("+")
        table = {
            "nodraw": 1 << 2, "skin1": 1 << 0, "skin2": 1 << 1,
            "crossfade": 1 << 6, "nodamage": 1 << 7, "nomipmaps": 1 << 8,
            "nopicmip": 1 << 9,
        }
        return table.get(w, 0)

    def _parse_init(self, tok, st):
        side = None
        depth = 0
        cur = None
        while True:
            t = tok.next()
            if t is None:
                return
            low = t.lower()
            if low in ("server", "client") and depth == 0:
                side = low
                continue
            if low == "{":
                depth += 1
                continue
            if low == "}":
                depth -= 1
                if depth <= 0:
                    return
                continue
            if low == "surface" and side == "server":
                name = tok.next()
                flag = tok.next() or ""
                st["init_server_surfaces"].append((name, flag))
                cur = flag
                continue


_CMD_WORDS = {
    "surface", "skelmodel", "morphfile", "path", "texturepath", "lod", "radius",
    "scale", "headoffset", "ischaracter", "replacesurface", "bonemapping",
    "lod_start_distance", "fade_dist_mod", "boundingvolumemin",
    "boundingvolumemax", "loadallanims", "loadallanims", "$include", "init",
    "server", "client", "classname", "name", "archetype", "origin", "lightbox",
    "notexturepath", "Damage", "damage",
}


# --------------------------------------------------------------------------
# resolution against the retail asset universe
# --------------------------------------------------------------------------

_SHADER_NAME_RE = re.compile(r"^([A-Za-z0-9_\-\.\\/]+)\s*\{", re.M)


def build_shader_universe(paks):
    """Every name declared at column 0 of a shaders/*.shader script."""
    names = set()
    for f in paks.glob("shaders/*.shader"):
        try:
            text = paks.read(f).decode("latin-1")
        except Exception:
            continue
        # strip comments so `// foo {` never registers
        text = re.sub(r"//[^\n]*", "", text)
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        for m in re.finditer(r"^\s*([^\s{}]+?)\s*\{", text, re.M):
            names.add(m.group(1).lower())
    return names


def resolve(paks, shader_universe, name):
    """Return (status, candidates_tried).

    status: 'image'  -> a real image file exists for this name
            'shader' -> a shaders/*.shader entry declares it
            'missing'-> nothing at all
    Mirrors retail R_LoadImage: a name ending in .tga is retried as .dds
    (buf[len-3..len-1] = "dds"), then the .tga itself; any other extensioned
    name is tried verbatim and has NO other fallback.
    """
    low = name.lower()
    cands = []
    if low.endswith(".tga"):
        cands.append(low[:-3] + "dds")
        cands.append(low)
    elif "." in posixpath.basename(low):
        cands.append(low)
    else:
        # extension-less: a shader-script name, or a bare image path
        if low in shader_universe:
            return "shader", [low + "  (shader script)"]
        cands.append(low + ".dds")
        cands.append(low + ".tga")
        for c in cands:
            if paks.exists(c):
                return "image", [c]
        return "missing", cands

    for c in cands:
        if paks.exists(c):
            return "image", [c]
    return "missing", cands


# --------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default=DEFAULT_BASE)
    ap.add_argument("--json", default=None)
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--max", type=int, default=0)
    args = ap.parse_args()

    paks = PakSet(args.base)
    uni = build_shader_universe(paks)
    sys.stderr.write("shaders/*.shader names: %d\n" % len(uni))

    tikis = [p for p in paks.glob("*.tik") if not p.startswith("models/sharedanim/")]
    tikis += paks.glob("models/sharedanim/*.tik")
    tikis = sorted(set(tikis))
    if args.max:
        tikis = tikis[: args.max]
    sys.stderr.write(".tik files: %d\n" % len(tikis))

    parser = TikParser(paks)
    report = []
    n_white_surfaces = 0
    n_files_hit = 0

    for tk in tikis:
        st = parser.load(tk)
        if st is None:
            report.append({"tik": tk, "error": "unreadable"})
            continue
        bad = []
        for name in st["order"]:
            ent = st["surfaces"][name]
            if not ent["shaders"]:
                # no shader directive at all -> the 2.0 tail promotes the
                # surface name to a shader, so test the surface name instead
                probes = [name]
                kinds = [resolve(paks, uni, name)[0] for name in probes]
            else:
                probes = ent["shaders"]
                kinds = [resolve(paks, uni, s)[0] for s in probes]
            if all(k == "missing" for k in kinds):
                n_white_surfaces += 1
                bad.append({
                    "surface": name,
                    "shaders": probes,
                    "flags": ent["flags"],
                    "tried": resolve(paks, uni, probes[0])[1] if probes else [name],
                })
        if bad:
            n_files_hit += 1
            report.append({
                "tik": tk, "num_surfaces": len(st["order"]),
                "unresolved": bad,
                "init_server_nodraw": [n for n, f in st["init_server_surfaces"]
                                       if f.lstrip("+").lower() == "nodraw"],
            })
        elif args.verbose:
            report.append({"tik": tk, "num_surfaces": len(st["order"]),
                           "unresolved": []})

    summary = {
        "base": args.base,
        "tik_files": len(tikis),
        "files_with_unresolved_surface": n_files_hit,
        "unresolved_surfaces_total": n_white_surfaces,
        "missing_includes": sorted(parser.missing_includes)[:40],
        "entries": report,
    }
    if args.json:
        with open(args.json, "w") as fh:
            json.dump(summary, fh, indent=1)
    else:
        json.dump(summary, sys.stdout, indent=1)

    sys.stderr.write(
        "\nfiles with >=1 unresolved surface: %d / %d\n"
        "unresolved surfaces total:        %d\n"
        % (n_files_hit, len(tikis), n_white_surfaces)
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
