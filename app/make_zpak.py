#!/usr/bin/env python3
"""
make_zpak.py -- build the port's own Android pak: app/src/main/assets/base/zpak-android.pk3

WHY THIS EXISTS
---------------
The rule is that the port must have exactly ONE cfg at launch -- the unified state
`config.cfg` in the engine's write root -- with the defaults folded in once from
`code/qcommon/ef2_android_defaults.h`.

This pak carries the three cfg names the data can ask for: the two the shipped
menus exec by name (the "Restore Controls" and "Restore Defaults" payloads)

    loc/Eng/ui/menu_promptrestorecontrols.urc: exec global/defaultcontrols.cfg
    loc/Eng/ui/menu_promptrestoredefaults.urc: exec global/defaultoptions.cfg

and `global/default.cfg` itself.  All three are generated from the SAME
`EF2_ANDROID_DEFAULT_COMMANDS` text the engine applies as its first-boot seed, so
they cannot drift from the engine's own defaults.

`global/default.cfg` is kept IN THE PAK even though the engine no longer reads it,
because the pak and the binary version independently: the run tree links this pak
from the source tree while the binary comes from whatever build directory the
launcher points at, so a binary of ANY vintage must boot against it.  Binaries up
to `c8d4dd90` still exec the path and still Com_Error on it in
`FS_InitFilesystem`/`FS_Restart`; from `c8d4dd90` on, no boot cfg is exec'd (the
seed is compiled in) and neither check exists.  Dropping the entry broke exactly
that: a stale binary plus this pak aborted with "Com_Error: FATAL: Couldn't load
default.cfg" on 2026-10-15 09:03.

What is gone is the LOOSE source, which is the one that mattered: the launcher
used to copy retail's `base/global/*.cfg` into the run tree, and under
loose-before-paks that copy shadowed this pak's files -- which is how the Linux
settings menu grew the Touch section.

The pak is DETERMINISTIC: fixed timestamps and sorted entries, so regenerating it
without a source change produces a byte-identical file and does not force an asset
re-extraction.

Usage:
    python3 app/make_zpak.py            # regenerate the pak
    python3 app/make_zpak.py --check     # verify it is current, exit 1 if not
"""
import argparse
import hashlib
import os
import re
import sys
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join(REPO, "app/src/main/cpp/code/qcommon/ef2_android_defaults.h")
PAK = os.path.join(REPO, "app/src/main/assets/base/zpak-android.pk3")
ASSET_DIR = os.path.join(REPO, "app/src/main/assets/base")
# Port-authored `loc/' override files, named explicitly.  cpp/loc/ also holds retail
# content (Eng/ui/background.inc came in with the port tree) and that must NOT be
# packed: this pak sorts ahead of every retail pak, so a retail file here would
# silently override the retail copy instead of only adding port-authored text.
LOC_DIR = os.path.join(REPO, "app/src/main/cpp/loc")
LOC_FILES = ("Eng/stringresource/globalstrings1.loc",)
# Fixed DOS timestamp (2026-01-01 00:00:00) so the archive is reproducible.
FIXED_DATE = (2026, 1, 1, 0, 0, 0)


def default_commands():
    """The EF2_ANDROID_DEFAULT_COMMANDS text, parsed out of the engine header."""
    src = open(HEADER, encoding="utf-8").read()
    m = re.search(r"#define\s+EF2_ANDROID_DEFAULT_COMMANDS\s*(.*?)\n\n", src, re.S)
    if not m:
        raise SystemExit("make_zpak: EF2_ANDROID_DEFAULT_COMMANDS not found in " + HEADER)
    body = m.group(1)
    parts = re.findall(r'"((?:[^"\\]|\\.)*)"', body)
    if not parts:
        raise SystemExit("make_zpak: no string literals in EF2_ANDROID_DEFAULT_COMMANDS")
    text = "".join(parts)
    # The literals are C escapes; the only ones the defaults use are \n and \".
    text = text.replace("\\n", "\n").replace('\\"', '"').replace("\\\\", "\\")
    if not text.endswith("\n"):
        text += "\n"
    if "seta cl_renderer" not in text:
        raise SystemExit("make_zpak: the parsed defaults look wrong (no cl_renderer)")
    return text


def collect():
    """Every entry the pak must contain, as (name, bytes)."""
    entries = {}
    # Port-authored loc data: front-end text the retail tables word for a mouse and
    # keyboard.  Each file here carries only the tokens it rewords, never a copy of a
    # retail table, and it wins because globalstrings1.loc loads inside
    # StringResource::loadGlobalStrings' 1..9 loop while _overwrite is still true --
    # retail ships only globalstrings.loc, so the name collides with nothing.
    for rel in LOC_FILES:
        src = os.path.join(LOC_DIR, rel)
        if not os.path.isfile(src):
            raise SystemExit("make_zpak: missing loc override %s" % src)
        entries["loc/" + rel] = open(src, "rb").read()
    # Only the 32-bit .tga touch art plus our defaults cfg.  The .png siblings that
    # sit in the assets directory are pre-uivk leftovers: retail's image resolver has
    # no .png loader (the note in cl_scrn.cpp's CL_TouchRegisterShaders), so shipping
    # them would add dead weight to every install.
    for f in sorted(os.listdir(os.path.join(ASSET_DIR, "gfx/touch"))):
        if not f.endswith(".tga"):
            continue
        path = os.path.join(ASSET_DIR, "gfx/touch", f)
        entries["gfx/touch/" + f] = open(path, "rb").read()
    # THREE names, all from the same header text: the two the shipped "Restore
    # Controls" / "Restore Defaults" prompt menus exec by name, and the boot
    # baseline `global/default.cfg` that binaries up to c8d4dd90 still exec and
    # still Com_Error on.  The pak is read by whatever binary the launcher points
    # at, so the baseline stays packed even though the current engine has no
    # reader for it (see the module docstring).
    text = default_commands()
    entries["global/default.cfg"] = ("unbindall\n" + text).encode("utf-8")
    lines = [l for l in text.split("\n") if l.strip()]
    binds = [l for l in lines if l.startswith("bind ") or l == "unbindall"]
    cvars = [l for l in lines if l.startswith(("seta ", "set "))]
    entries["global/defaultcontrols.cfg"] = ("\n".join(["unbindall"] + binds) + "\n").encode("utf-8")
    entries["global/defaultoptions.cfg"] = ("\n".join(cvars) + "\n").encode("utf-8")
    return entries


def build(entries, path):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        for name in sorted(entries):
            info = zipfile.ZipInfo(name, FIXED_DATE)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, entries[name])


def current_entries(path):
    if not os.path.exists(path):
        return {}
    with zipfile.ZipFile(path) as z:
        return {n: z.read(n) for n in z.namelist()}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="verify the pak on disk matches the sources; do not write")
    args = ap.parse_args()

    entries = collect()
    if args.check:
        want = {n: hashlib.sha256(b).hexdigest() for n, b in entries.items()}
        have = {n: hashlib.sha256(b).hexdigest() for n, b in current_entries(PAK).items()}
        if want != have:
            print("make_zpak: the pak is STALE")
            for n in sorted(set(want) | set(have)):
                if want.get(n) != have.get(n):
                    print("   differs: %s" % n)
            return 1
        print("make_zpak: pak is current (%d entries)" % len(entries))
        return 0

    build(entries, PAK)
    print("make_zpak: wrote %s (%d entries, %d bytes)" % (
        os.path.relpath(PAK, REPO), len(entries), os.path.getsize(PAK)))
    for name in sorted(entries):
        print("   %8d  %s" % (len(entries[name]), name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
