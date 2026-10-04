#!/usr/bin/env python3
"""The config wiring: which value the engine actually loads, and what the
launcher's per-boot override does to it.

Three facts carry the launch/config chain, all checkable against the shipping
sources and the launcher on disk:

  1. the ordering in Com_Init: the command line is applied before
     FS_InitFilesystem, config.cfg is exec'd, the defaults seed runs, the
     launcher's override file is exec'd through `com_execAfterConfig`, and only
     then is the command line re-applied -- so the final precedence is
     compiled default < config.cfg < the override file < an explicit +set;
  2. the command line has 32 slots (Com_ParseCommandLine stops at the 32nd),
     so a settings block cannot travel as +set and the launcher must leave
     headroom for the caller's own arguments;
  3. a value that reaches the archive comes back on the next boot, so a
     per-boot override has to travel as `setx', which Cvar_WriteVariables
     skips.

The launcher is read from the tree it lives in when that tree has it, and from
the fixture copy here otherwise, so the control stays green after the branch
merges and still audits the real script today.

Modes:
  0                  the shipping tree and the launcher must be green
  1..N               a planted fault must be detected
  <files...>         the same checks against mutated copies (source faults)
Never launches the game.
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
CODE = os.path.normpath(os.path.join(HERE, "..", "..", "..",
                                     "app", "src", "main", "cpp", "code"))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", ".."))

Q_SHARED = os.path.join("qcommon", "q_shared.h")
CVAR_C = os.path.join("qcommon", "cvar.c")
COMMON_C = os.path.join("qcommon", "common.c")
VK_PLATFORM = os.path.join("renderervulkan", "vk_platform.c")
VK_SWAPCHAIN = os.path.join("renderervulkan", "vk_swapchain.c")
TR_INIT = os.path.join("renderervulkan", "tr_init.c")

# the keys the launcher owns: the engine must not archive them behind the
# launcher's back, and the launcher must set every one of them each boot
REQUIRED_KEYS = ("cl_renderer", "r_mode", "r_fullscreen", "com_maxfps",
                 "r_smp", "r_vkSafeMode", "r_lightmap", "logfile")

MAX_CONSOLE_LINES = 32


def read(path):
    with open(path, encoding="utf8", errors="replace") as fh:
        return fh.read()


def launcher_text():
    live = os.environ.get("EF2_LAUNCHER_PATH") or os.path.join(
        REPO, "personal", "scripts", "play_ef2_decomp.sh")
    if os.path.isfile(live):
        return read(live), live
    return read(os.path.join(HERE, "play_ef2_decomp_fixture.sh")), "fixture"


checks = 0
failures = 0
notes = []


def check(cond, msg):
    global checks, failures
    checks += 1
    if not cond:
        failures += 1
        print("  FAIL  %s" % msg)
    return cond


# --- 1. the engine's ordering -------------------------------------------------

def check_engine(files):
    cvar = files[CVAR_C]
    common = files[COMMON_C]
    shared = files[Q_SHARED]
    vksw = files[VK_SWAPCHAIN]
    trinit = files[TR_INIT]

    # the override slot exists, is exec'd after the config exec and the seed,
    # and is guarded on the file's existence
    i_cfg = common.find('Cbuf_AddText( va( "exec %s\\n", configname ) )')
    i_ov = common.find('Cvar_VariableString( "com_execAfterConfig" )')
    check(i_cfg >= 0, "common.c: the config exec site moved")
    check(i_ov > i_cfg, "common.c: the override exec must follow the config exec")
    i_seed = common.find("EF2_ANDROID_DEFAULT_COMMANDS")
    check(i_seed >= 0 and i_ov > i_seed,
          "common.c: the override exec must follow the defaults seed")
    i_cmdline = common.rfind("Com_StartupVariable( NULL );", 0, len(common))
    check(i_cmdline > i_ov,
          "common.c: the command line must be re-applied after the override exec")
    check("FS_FileExists_HomeData( overrideCfg )" in common,
          "common.c: the override exec must be guarded on the file existing")

    # the cvar is TEMP: it must never reach the archive
    check(re.search(r'Cvar_Get\(\s*"com_execAfterConfig",\s*"",\s*CVAR_TEMP\s*\)', common) is not None,
          "common.c: com_execAfterConfig must be registered CVAR_TEMP")

    # setx exists, is registered, and marks the cvar
    check("Cvar_SetX_f" in cvar, "cvar.c: setx is gone")
    check('Cmd_AddCommand ("setx", Cvar_SetX_f);' in cvar, "cvar.c: setx is not registered")
    check("CVAR_NOARCHIVE" in shared, "q_shared.h: CVAR_NOARCHIVE is gone")
    m = re.search(r"void Cvar_SetX_f\( void \) \{(.*?)\n\}", cvar, re.S)
    check(m is not None and "CVAR_NOARCHIVE" in m.group(1),
          "cvar.c: setx no longer marks the cvar CVAR_NOARCHIVE")

    # the archive honours it
    m = re.search(r"void Cvar_WriteVariables\(fileHandle_t f\)\s*\{(.*?)\n\}", cvar, re.S)
    check(m is not None, "cvar.c: Cvar_WriteVariables moved")
    if m:
        check("CVAR_NOARCHIVE" in m.group(1),
              "cvar.c: the archive no longer skips a CVAR_NOARCHIVE cvar")

    # the command line sets with force: retail Com_StartupVariable @0x000C87A0
    # goes through Cvar_Set = Cvar_Set2(force=1)
    m = re.search(r"void Com_StartupVariable\( const char \*match \) \{(.*?)\n\}", common, re.S)
    check(m is not None, "common.c: Com_StartupVariable moved")
    if m:
        body = m.group(1)
        check(re.search(r"Cvar_Set2\(\s*s,\s*Cmd_ArgsFrom\(2\),\s*qtrue\s*\)", body) is not None,
              "common.c: Com_StartupVariable no longer sets with force")

    # the command line cap is retail's 32 and still silently stops
    m = re.search(r"#define\s+MAX_CONSOLE_LINES\s+(\d+)", common)
    check(m is not None and int(m.group(1)) == MAX_CONSOLE_LINES,
          "common.c: MAX_CONSOLE_LINES is not %d" % MAX_CONSOLE_LINES)

    # the present mode honours r_swapInterval: the launcher's "no vsync" is real
    m = re.search(r"(static void VK_PickPresentMode\(.*?\n\})", vksw, re.S)
    check(m is not None, "vk_swapchain.c: VK_PickPresentMode moved")
    if m:
        check("r_swapInterval" in m.group(1),
              "vk_swapchain.c: the present mode no longer reads r_swapInterval")

    # the safe-mode guard names its cause
    check(re.search(r'cause=%s",\s*\n?\s*s_vkSafeMode->integer \? "explicit" : "prior-crash"', trinit) is not None,
          "tr_init.c: the safe-mode guard row no longer names its cause")


# --- 2. the launcher ----------------------------------------------------------

def parse_launcher(text):
    """the launcher's own settings block (including the mode appends) and its
    +set command line"""
    # comment lines first: a paren in a comment would end the block early
    nocomment = "\n".join(l for l in text.split("\n") if not l.lstrip().startswith("#"))
    blocks = re.findall(r"LAUNCHER_OVERRIDES(?:=|\+=)\(([^)]*)\)", nocomment, re.S)
    if not check(len(blocks) >= 1, "launcher: LAUNCHER_OVERRIDES block not found"):
        return None, None, None
    if not check(len(blocks) >= 2,
                 "launcher: the diagnostic mode appends are missing from the settings block"):
        blocks = blocks or [""]
    block = "\n".join(blocks)
    keys = []
    for line in block.split("\n"):
        line = line.split("#")[0].strip()
        for kv in re.findall(r'"([A-Za-z_][A-Za-z0-9_]*=[^"]*)"', line):
            k, v = kv.split("=", 1)
            keys.append((k, v.strip()))
    m = re.search(r"printf 'setx %s %s\\n'", text)
    writer = "setx" if m else None
    # the engine command line: from the binary to the log redirect
    cmdline = ""
    i = text.rfind("./provenanceef2")   # the launch, not a comment or an echo
    if i >= 0:
        rest = text[i:]
        j = rest.find("LOGFILE")
        if j >= 0:
            k = rest.rfind(">", 0, j)   # drop the redirect and its quote
            cmdline = rest[:k]
    return keys, writer, cmdline


def check_launcher(text):
    keys, writer, cmdline = parse_launcher(text)
    if not check(keys is not None, "launcher: no settings block"):
        return
    names = [k for k, _ in keys]
    # the two mode branches are alternatives, so a key may appear once per branch
    perbranch = [n for n, c in __import__("collections").Counter(names).items() if c > 2]
    check(not perbranch, "launcher: a key is set more than once per branch: %s" % perbranch)
    for k in REQUIRED_KEYS:
        check(k in names, "launcher: the required key %s is not in the settings block" % k)
    check(writer == "setx",
          "launcher: the override file must be written with setx, not %s" % writer)
    check("setx" in text and "seta" not in re.sub(r"#.*", "", text.split("OVERRIDE_FILE")[1])
          if "OVERRIDE_FILE" in text else True,
          "launcher: the override file must not use seta")

    # the command line: only fs_basepath/fs_homepath and the override pointer,
    # and always enough room under the 32-slot cap for the caller's own args
    check(cmdline is not None, "launcher: the engine command line was not found")
    if cmdline:
        slots = len(re.findall(r"\+set\s+[A-Za-z_][A-Za-z0-9_]*", cmdline)) + 1
        check(slots <= MAX_CONSOLE_LINES - 8,
              "launcher: %d of %d command-line slots used, the caller needs headroom"
              % (slots, MAX_CONSOLE_LINES))
        check("+set fs_basepath" in cmdline and "+set fs_homepath" in cmdline,
              "launcher: fs_basepath/fs_homepath must stay on the command line "
              "(FS_InitFilesystem runs before any config exec)")
        check("+set com_execAfterConfig" in cmdline,
              "launcher: the override file is not named on the command line")
        check("+set cl_renderer" not in cmdline,
              "launcher: cl_renderer belongs to the override file, not the command line")


# --- the precedence model -----------------------------------------------------

def model_precedence():
    """compiled default < config.cfg < the override file < an explicit +set"""
    global checks, failures

    def resolve(default, config, override, cmdline):
        val = default
        if config is not None:
            val = config
        if override is not None:
            val = override
        if cmdline is not None:
            val = cmdline
        return val

    cases = [
        ("config only", dict(default="1", config="2", override=None, cmdline=None), "2"),
        ("override beats config", dict(default="1", config="2", override="3", cmdline=None), "3"),
        ("the command line beats the override", dict(default="1", config="2", override="3", cmdline="4"), "4"),
        ("the command line beats the config", dict(default="1", config="2", override=None, cmdline="4"), "4"),
        ("no config, no override", dict(default="1", config=None, override=None, cmdline=None), "1"),
    ]
    for name, args, want in cases:
        check(resolve(**args) == want, "precedence model: %s resolved to %s, want %s"
              % (name, resolve(**args), want))


# --- the command-line slot model ---------------------------------------------

def model_slot_cap():
    """Com_ParseCommandLine stops at the 32nd argument and drops the rest"""
    global checks, failures

    def parse(line):
        """Com_ParseCommandLine: split at every '+', stop at the 32nd"""
        out = [""]
        cur = []
        inq = False
        for ch in line:
            if ch == '"':
                inq = not inq
            if ch == "+" and not inq:
                if len(out) == MAX_CONSOLE_LINES:
                    return out
                out.append("")
                cur = out[len(out) - 1:]
                continue
            out[-1] += ch
        return out

    base = "+set fs_basepath /tmp +set fs_homepath /tmp"
    sets = " ".join("+set r_a %d" % i for i in range(40))
    lines = parse(base + " " + sets)
    check(len(lines) == MAX_CONSOLE_LINES,
          "slot model: the cap did not stop the parse (%d)" % len(lines))
    applied = sum(1 for l in lines if l.startswith("set "))
    check(applied < 40, "slot model: %d of 40 sets survived the cap" % applied)
    # the launcher's own line must fit with room for the caller's overrides
    keys, _, cmdline = parse_launcher(launcher_text()[0])
    if cmdline:
        own = len(re.findall(r"\+set\s+[A-Za-z_][A-Za-z0-9_]*", cmdline))
        flat = " ".join(part.strip() for part in cmdline.split("\\"))
        lines = parse(flat + " " + " ".join("+set r_x %d" % i for i in range(8)))
        survived = sum(1 for l in lines if l.startswith("set r_x"))
        check(own + 8 <= MAX_CONSOLE_LINES and survived == 8,
              "slot model: 8 caller overrides do not survive (own=%d, survived=%d)"
              % (own, survived))


# --- planted faults -----------------------------------------------------------

def source_fault(name, mutate, *paths):
    global checks, failures
    tmp = tempfile.mkdtemp()
    try:
        files = {}
        for rel in (Q_SHARED, CVAR_C, COMMON_C, VK_PLATFORM, VK_SWAPCHAIN, TR_INIT):
            src = os.path.join(CODE, rel)
            dst = os.path.join(tmp, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy(src, dst)
            files[rel] = read(dst)
        for rel in paths:
            files[rel] = mutate(files[rel])
        before = (checks, failures)
        check_engine(files)
        if failures == before[1]:
            failures += 1
            print("  FAIL  source fault %s was not detected" % name)
        else:
            print("  ok    source fault %s detected" % name)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def strip_last(text, needle, count):
    for _ in range(count):
        i = text.rfind(needle)
        if i < 0:
            return text
        text = text[:i] + text[i + len(needle):]
    return text


def launcher_fault(name, text_mut):
    global checks, failures
    before = (checks, failures)
    check_launcher(text_mut(launcher_text()[0]))
    if failures == before[1]:
        failures += 1
        print("  FAIL  launcher fault %s was not detected" % name)
    else:
        print("  ok    launcher fault %s detected" % name)


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "0"
    paths = sys.argv[2:]

    files = {}
    for rel in (Q_SHARED, CVAR_C, COMMON_C, VK_PLATFORM, VK_SWAPCHAIN, TR_INIT):
        files[rel] = read(os.path.join(CODE, rel))
    if paths:
        for rel in paths:
            key = rel
            if not os.path.isabs(rel):
                key = os.path.relpath(rel, CODE)
            files[key] = read(rel)
        check_engine(files)
        print("  %d checks, %d failures" % (checks, failures))
        return 1 if failures else 0

    text, where = launcher_text()

    check_engine(files)
    check_launcher(text)
    model_precedence()
    model_slot_cap()

    if mode == "0":
        print("  launcher read from %s" % where)
        print("  %d checks, %d failures" % (checks, failures))
        return 1 if failures else 0

    faults = {
        "1": ("the archive keeps a setx cvar",
              lambda s: s.replace("\t\tif( var->flags & CVAR_NOARCHIVE )\n\t\t\tcontinue;\n", ""),
              [CVAR_C]),
        "2": ("setx stops marking the cvar",
              lambda s: s.replace("\tv->flags |= CVAR_NOARCHIVE;", "\tv->flags |= CVAR_ARCHIVE;"),
              [CVAR_C]),
        "3": ("setx is not registered",
              lambda s: s.replace('Cmd_AddCommand ("setx", Cvar_SetX_f);', ""),
              [CVAR_C]),
        "4": ("the command line stops setting with force",
              lambda s: s.replace("Cvar_Set2( s, Cmd_ArgsFrom(2), qtrue );",
                                  "Cvar_Set2( s, Cmd_ArgsFrom(2), qfalse );"),
              [COMMON_C]),
        "5": ("the command line is no longer re-applied after the override",
              lambda s: strip_last(s, "Com_StartupVariable( NULL );", 2),   # the two post-config passes
              [COMMON_C]),
    }
    if mode in faults:
        name, mut, rels = faults[mode]
        source_fault("mode %s: %s" % (mode, name), mut, *rels)
        print("  %d checks, %d failures" % (checks, failures))
        return 1 if failures else 0

    if mode == "6":
        launcher_fault(mode, lambda s: s.replace("printf 'setx %s %s\\n'", "printf 'seta %s %s\\n'"))
    elif mode == "7":
        launcher_fault(mode, lambda s: s.replace('"r_smp=0"  ', ""))
    elif mode == "8":
        launcher_fault(mode, lambda s: s.replace("+set com_execAfterConfig ef2-launcher.cfg \\", ""))
    elif mode == "9":
        # the shape this replaced: the whole block travelling as +set, which the
        # command line has no room for
        launcher_fault(mode, lambda s: s.replace(
            "+set com_execAfterConfig ef2-launcher.cfg \\",
            " ".join("+set pad%02d 1 \\" % i for i in range(30))))
    else:
        print("unknown mode %s" % mode)
        return 2

    print("  %d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
