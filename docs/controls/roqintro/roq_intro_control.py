#!/usr/bin/env python3
"""
roq_intro_control.py -- the intro RoQ chain's offline control.  Never launches
the game.

The intro is not code: it is one `setd' line in retail's `global/game.cfg'
(pak0), read by exactly one function.

    Com_Init            exec global/game.cfg        (before config.cfg)
    global/game.cfg     setd introCommand "defaultSoundTrack none;
                            cinematic paramount.roq; cinematic activision.roq;
                            cinematic ritual.roq; cinematic intro.roq;
                            afterCinematic defaultSoundTrack music/intro;
                            afterCinematic pushmenu main"
    UI_Init             Cbuf_AddText("dointro")
    UI_StartIntro_f     cmd = CvarGetForUI("introCommand"); Cbuf_AddText(cmd)
    CL_PlayCinematic_f  CIN_PlayCinematic("movies/<name>", ...)

Remove the exec and `introCommand' is never set, `dointro' execs nothing, and the
screen stays black until ESC -- which is what happened on 2026-10-15 09:06, the
first operator run whose binary did not exec it.  This control asserts every link,
from the shipped paks to the built binary's own string table, and goes RED on a
planted fault at each link.

Build/run: docs/controls/roqintro/run.sh
"""
import os
import re
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
WT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
CODE = os.path.join(WT, "app", "src", "main", "cpp", "code")
BINARY = os.path.join(WT, "app", "src", "main", "cpp", "build", "Release", "provenanceef2")
COMMON = os.path.join(CODE, "qcommon", "common.c")
CL_UI = os.path.join(CODE, "client", "cl_ui.cpp")
CL_MAIN = os.path.join(CODE, "client", "cl_main.cpp")
CL_CIN = os.path.join(CODE, "client", "cl_cin.cpp")

# the retail install; override with EF2_RETAIL_ROOT
RETAIL = os.environ.get("EF2_RETAIL_ROOT", os.path.expanduser("~/Star Trek Elite Force II"))

failures = 0
checks = 0


def check(cond, msg):
    global failures, checks
    checks += 1
    if not cond:
        failures += 1
        print("  FAIL  " + msg)


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def mutate(text, old, new):
    """A planted fault: the text must contain old exactly once."""
    assert text.count(old) == 1, "mutant anchor not unique: %r (%d)" % (old, text.count(old))
    return text.replace(old, new)


def pak_census(root):
    """Every entry name in every retail pak, and the pak a name lives in."""
    names = {}
    for f in sorted(os.listdir(os.path.join(root, "base"))):
        if not f.endswith(".pk3"):
            continue
        with zipfile.ZipFile(os.path.join(root, "base", f)) as z:
            for n in z.namelist():
                names.setdefault(n.lower(), (f, n))
    return names


def main():
    mutant = int(sys.argv[1]) if len(sys.argv) > 1 else 0

    common = read(COMMON)
    cl_ui = read(CL_UI)
    cl_main = read(CL_MAIN)
    cl_cin = read(CL_CIN)

    # --- planted faults in the source chain ---------------------------------
    if mutant == 1:
        # the exec removed: the "no boot cfg is exec'd" regression
        common = mutate(common, '\tCbuf_AddText( "exec global/game.cfg\\n" );\n', "")
    if mutant == 2:
        # the exec moved after the config exec: the archive wins the wrong way
        common = mutate(common, '\tCbuf_AddText( "exec global/game.cfg\\n" );\n\tCbuf_Execute( 0 );\n\tCmd_RemoveCommand( "setd" );\n\n\n', "")
    if mutant == 3:
        # the cinematic command unregistered
        cl_main = mutate(cl_main, 'Cmd_AddCommand ("cinematic", CL_PlayCinematic_f);',
                         '/* unregistered */')
    if mutant == 4:
        # dointro never queued
        cl_ui = mutate(cl_ui, 'Cbuf_AddText("dointro\\n");', '/* no intro */')

    # --- 1. the shipped data: game.cfg's introCommand ------------------------
    pak_dir = os.path.join(RETAIL, "base")
    have_paks = os.path.isdir(pak_dir)
    check(have_paks, "retail install not found at %s (set EF2_RETAIL_ROOT)" % RETAIL)

    game_cfg = None
    census = {}
    if have_paks:
        census = pak_census(RETAIL)
        hit = census.get("global/game.cfg")
        check(hit is not None, "no global/game.cfg in any retail pak")
        if hit:
            with zipfile.ZipFile(os.path.join(pak_dir, hit[0])) as z:
                game_cfg = z.read(hit[1]).decode("latin-1", "replace")

    # --- planted faults in the data -----------------------------------------
    if mutant == 5 and game_cfg:
        game_cfg = re.sub(r'setd introCommand.*', 'setd introCommand ""', game_cfg)

    if game_cfg is not None:
        m = re.search(r'setd\s+introCommand\s+"([^"]*)"', game_cfg)
        check(m is not None, "global/game.cfg has no `setd introCommand' line")
        if m:
            value = m.group(1)
            check(len(value.strip()) > 0, "introCommand is empty")
            order = [n for n in ("paramount", "activision", "ritual", "intro")
                     if ("cinematic %s.roq" % n) in value]
            check(order == ["paramount", "activision", "ritual", "intro"],
                  "introCommand does not run the four logos in retail's order: %r" % order)
            check("pushmenu main" in value,
                  "introCommand does not end by pushing the main menu")
            # every roq it names must ship, under the engine's movies/ prefix
            for n in ("paramount", "activision", "ritual", "intro"):
                want = "movies/%s.roq" % n
                check(want in census, "introCommand names %s but no pak carries it" % want)
        # the menu model that the same file defines (the Controls background)
        mv = re.search(r'setd\s+ui_mainMenuModel\s+(\S+)', game_cfg)
        check(mv is not None, "global/game.cfg does not set ui_mainMenuModel")
        if mv:
            check(mv.group(1).lower() in census,
                  "ui_mainMenuModel %s is not in any pak" % mv.group(1))

    # --- 2. the source chain ------------------------------------------------
    i_game = common.find('Cbuf_AddText( "exec global/game.cfg\\n" );')
    i_cfg = common.find('Cbuf_AddText( va( "exec %s\\n", configname ) );')
    check(i_game >= 0, "common.c does not exec global/game.cfg")
    check(i_cfg >= 0, "common.c does not exec the config")
    check(0 <= i_game < i_cfg,
          "global/game.cfg is exec'd after config.cfg (retail execs it before, so a "
          "value setd by game.cfg would overwrite the archived config)")
    check(common.find('Cmd_AddCommand( "setd", Cvar_SetD_f );') >= 0
          and common.find('Cmd_AddCommand( "setd", Cvar_SetD_f );') < i_game if i_game > 0 else False,
          "`setd' is not registered before global/game.cfg is exec'd")
    check('Cmd_RemoveCommand( "setd" );' in common, "`setd' is never removed again")

    check('Cmd_AddCommand("dointro", UI_StartIntro_f)' in cl_ui,
          "cl_ui.cpp does not register `dointro'")
    check('Cbuf_AddText("dointro\\n");' in cl_ui, "cl_ui.cpp never queues `dointro'")
    check('CvarGetForUI("introCommand"' in cl_ui,
          "UI_StartIntro_f does not read introCommand")

    check('Cmd_AddCommand ("cinematic", CL_PlayCinematic_f);' in cl_main,
          "cl_main.cpp does not register `cinematic'")
    check("CIN_PlayCinematic(" in cl_cin, "CL_PlayCinematic_f does not call CIN_PlayCinematic")

    # --- 3. the built binary's own string table -----------------------------
    have_bin = os.path.isfile(BINARY)
    check(have_bin, "no build at %s (build first: ninja -C build)" % BINARY)
    if have_bin:
        with open(BINARY, "rb") as f:
            blob = f.read()
        for lit in (b"exec global/game.cfg", b"introCommand", b"dointro",
                    b"cinematic"):
            check(lit in blob, "the built binary does not carry the literal %r" % lit)

    print("roq intro control: %d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
