#!/usr/bin/env python3
"""
make_zpak.py -- build the port's own Android pak: app/src/main/assets/base/zpak-android.pk3

WHY THIS EXISTS
---------------
The operator's rule (2026-09-30) is that the port must have exactly ONE LOOSE cfg -- the
unified operator-state `config.cfg` in the engine's write root -- while cfgs that live
INSIDE pak files are fine to keep.

Retail keeps its key-bind and cvar defaults in a LOOSE `base/global/default.cfg`, and
the engine HARD-REQUIRES that path: `FS_InitFilesystem` and `FS_Restart` both end with
retail's own check

    if ( FS_ReadFile( "global/default.cfg", NULL ) <= 0 )
        Com_Error( ERR_FATAL, "Couldn't load default.cfg" );

so a minimal install that has only the retail paks (which is exactly what the port's
import flow asks for) crashes on the first launch.  That is what happened on the
operator's Pixel 1:

    Com_Error: FATAL: Couldn't load default.cfg
    ... assertion "code != ERR_FATAL" failed

This script puts a PORT-AUTHORED `global/default.cfg` -- the same command text the
engine already applies as `EF2_ANDROID_DEFAULT_COMMANDS`, parsed straight out of
`app/src/main/cpp/code/qcommon/ef2_android_defaults.h` so the two can never drift --
into the port's own pak.  The path then resolves from the pak, no retail data is
shipped or copied, no engine check is weakened, and the only loose cfg left is
`config.cfg`.  A retail install's LOOSE `global/default.cfg` still wins, because the
loose base directories are earlier in the search path than the paks (the device's own
`[R133:pakmount]` list prints slot 0/1 as directories and slot 2 as this pak), so
retail behaviour is preserved wherever retail shipped the file.

The pak is DETERMINISTIC: fixed timestamps and sorted entries, so regenerating it
without a source change produces a byte-identical file and does not force an asset
re-extraction.

Usage:
    python3 android/make_zpak.py            # regenerate the pak
    python3 android/make_zpak.py --check     # verify it is current, exit 1 if not
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
    # Only the 32-bit .tga touch art plus our defaults cfg.  The .png siblings that
    # sit in the assets directory are pre-r133 leftovers: retail's image resolver has
    # no .png loader (the note in cl_scrn.cpp's CL_TouchRegisterShaders), so shipping
    # them would add dead weight to every install.
    for f in sorted(os.listdir(os.path.join(ASSET_DIR, "gfx/touch"))):
        if not f.endswith(".tga"):
            continue
        path = os.path.join(ASSET_DIR, "gfx/touch", f)
        entries["gfx/touch/" + f] = open(path, "rb").read()
    # THREE names, because the shipped menus exec all three:
    #   * global/default.cfg          -- the boot baseline (retail's FS_InitFilesystem requires the path)
    #   * global/defaultcontrols.cfg  -- the "Restore Controls" button
    #        (loc/Eng/ui/menu_promptrestorecontrols.urc: stuffcommand "exec global/defaultcontrols.cfg")
    #   * global/defaultoptions.cfg   -- the "Restore Defaults" button
    #        (loc/Eng/ui/menu_promptrestoredefaults.urc: stuffcommand "exec global/defaultoptions.cfg")
    # All three are generated from the SAME header text, so they cannot drift from the
    # engine's own first-boot defaults.  Retail ships them as LOOSE files in base/global/;
    # the operator's rule is that the port has exactly ONE loose cfg, so they live here.
    text = default_commands()
    # Retail's default.cfg opens with `unbindall' (its own reset semantics); the
    # header macro stays a pure command list, so the generator adds it here.
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
