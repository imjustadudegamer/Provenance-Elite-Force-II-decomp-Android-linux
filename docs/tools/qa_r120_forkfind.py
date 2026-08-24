#!/usr/bin/env python3
"""
qa_r120_forkfind.py -- R120 log fork-finder (reusable, worktree-only).

Normalizes a port console log and a retail oracle capture against each other,
suppresses the ADJUDICATED retail-identical line classes so they cannot mis-fork
(the R117c lesson: raw-text equality plus a mid-load benign block equals a wrong
fork), and reports the FIRST structural divergence as an ordered line-CLASS
mismatch -- not a text diff.

It also independently walks the R120 probe protocol: the set of [R120:*] lines
each of the three chains MUST produce, in order, and names the first missing or
failing link.

USAGE
    python3 docs/tools/qa_r120_forkfind.py <port.log> [retail.golden]

    python3 docs/tools/qa_r120_forkfind.py \\
        scratchpad/logs/play_ef2-20260922-063024.log \\
        docs/oracle/golden/solomatch/retail.F

With no retail golden, only the probe walk and the port-side class census run.
Exit code 0 always; findings are printed to stdout.
"""

import re
import sys

# ---------------------------------------------------------------------------
# Adjudicated retail-identical classes -- SUPPRESSED so they cannot mis-fork.
# (R119 sec.1 / R117c sec.3 / R118 sec.7 decisions.  Do not add to this list
#  without a written adjudication.)
# ---------------------------------------------------------------------------
SUPPRESS = [
    # row 13 -- the texture is absent from retail data too (.tik flags nodraw)
    r"Couldn't find image for shader models/char/ferengi/oolpax/oolpax-face\.tga",
    # row 15 -- retail's own pak0 ammo_plasma.shader uses rgbGen add; retail prints it
    r"WARNING: unknown rgbGen parameter 'add'",
    # rows 11/12-adjacent -- mixed image parms are a data consequence, PRINT_WARNING
    r"WARNING: reused image .* with mixed (allowPicmip|glWrapClampMode|mipmap) parm",
    # row 12 (store caps) -- TIKI frame-command caps exist in retail
    r"TIKI_ParseFrameCommands: too many (anim commands|args)",
    # row 14 -- the WDPrintf line itself is developer-gated on BOTH sides
    r"Null entity referenced in ",
    r"Stack Trace follows:",
    r"^\S*\.scr : \S+",
    r"<NO FUNCTION>",
    r"^=+$",
    # retail-identical TIKI/engine chatter
    r"TIKI_GetMorph: handle -1 out of range",
    r"TIKI_LoadTikiModel: bonemapping bone .* not found",
    r"TIKI_CopySurfacesSkel: could not find surface",
    r"UIWidgetContainer::ValidEvent : Event .* out of response range",
]

# Ordered line CLASSES the two sides must agree on.  Each entry is
# (label, regex-over-normalized-port-line).  A class is "present" if the
# normalized port log contains at least one matching line.
PORT_CLASSES = [
    ("mapload",      r"(CM_LoadMap|Server: |Spawning Entities)"),
    ("gameso",       r"Sys_GetGameAPI\(game\.so\): succeeded"),
    ("cgameso",      r"Sys_GetCGameAPI\(cgame\.so\): succeeded"),
    ("entitycount",  r"\d+ entities spawned"),
    ("script",       r"Adding script: 'maps/\S+\.scr'"),
    ("cliententer",  r"\S+ entered the game"),
    ("clientjoin",   r"\S+ joined the game"),
    ("beamshader",   r"beamshader"),
    ("beams_shader", r"shaders/beams\.shader"),
    ("shutdown",     r"Server Shutdown"),
]

# ---------------------------------------------------------------------------
# R120 probe protocol -- what a correctly-functioning chain MUST emit.
# Each chain is an ordered list of (link-id, regex, "what it proves").
# The walk reports the first link with ZERO lines, or the first link whose
# content shows a failure marker.
# ---------------------------------------------------------------------------
PROBE_PROTOCOL = {
    "BEAM ORIGIN (row 12)": [
        ("B1 entupdate",  r"\[R120:beam:entupdate\]",  "an entity offers its emitters"),
        ("B2 emitmatch",  r"\[R120:beam:emitmatch\]",  "a beam emitter matched an entity+is on"),
        ("B3 orient",     r"\[R120:beam:orient\]",     "tag resolved against a refEntity (its origin!)"),
        ("B4 fparm",      r"\[R120:beam:fparm\]",      "the PORT-ONLY view-axis stand-in fired"),
        ("B5 updatebeam", r"\[R120:beam:updatebeam\]", "the per-frame start/end handed to CreateBeam"),
        ("B6 create",     r"\[R120:beam:create\]",     "CG_CreateBeam stored the beam"),
        ("B7 slot",       r"\[R120:beam:slot\]",       "cl_beams slot alive at draw time"),
        ("B8 addpoly",     r"\[R120:beam:addpoly\]",   "shader handle that reached the renderer"),
    ],
    "NPC SPAWN (solomatch)": [
        ("N1 initmp",     r"\[R120:npc:initmp\]",        "the MP layer came up (R117b seam)"),
        ("N2 addbots",    r"\[R120:npc:addbots\]",       "fgame read mp_botcommands"),
        ("N3 addbotentry", r"\[R120:npc:addbot-entry\]", "the export hand-off fired (R120 fix)"),
        ("N4 warmup",     r"\[R120:npc:warmup\]",        "warmup clock / min-players decision"),
        ("N5 matchstart", r"\[R120:npc:matchstart\]",    "startMatch ran -> bots spawn"),
    ],
    "INVENTORY BAR + KEYS (rows 16/17)": [
        ("I1 bindexec",   r"\[R120:input:bindexec\]",    "a keydown reached the binding executor"),
        ("I2 swallowed",  r"\[R120:input:swallowed\]",   "a keydown was eaten by the UI catcher"),
        ("I3 invcmd",     r"\[R120:inv:cmd\]",           "warpinv/nextinv reached its handler"),
        ("I4 gwc",        r"\[R120:inv:gwc\]",           "globalwidgetcommand entered delivery"),
        ("I5 deliver",    r"\[R120:inv:deliver\]",       "the named widget was reached"),
        ("I6 showmenu",   r"\[R120:inv:showmenu\]",      "a DISABLED widget was silently skipped"),
        ("I7 bar",        r"\[R120:inv:bar\]",           "the bar's own gates + reject arm"),
        ("I8 mirror",     r"\[R120:inv:mirror\]",        "the client inventory mirror population"),
        ("I9 mirrorsv",   r"\[R120:inv:mirror-sv\]",     "the SERVER publishing items"),
    ],
}

FAILURE_MARKERS = [
    ("GATE m_enabled==0",          "the bar's enabled flag is off"),
    ("GATE view3d+focus",          "view3d/focus gate dropped the event"),
    ("REJECT type",                "client inventory mirror is EMPTY (decider 1)"),
    ("MIRROR EMPTY",               "client inventory mirror is EMPTY (decider 1)"),
    ("bot_enable=0 -- bot refused", "bot_enable cvar is off"),
    ("BLOCKED minPlayers",         "mp_minPlayers is holding the match in warmup"),
]

# ---------------------------------------------------------------------------
# R131 -- the RETAIL DUMP WORKSTREAM pairings (docs/oracle/shim/oracle_log.c SECTION
# 11).  A capture taken with EF2_ORACLE_DUMPS set carries `# retail <family>`
# records; each one is the RETAIL measurement of a port probe this workstream cannot
# take at the contract boundary.  `pairings()` joins them by `f=` (the per-side
# frame counter) and prints the verdict per open item.
# ---------------------------------------------------------------------------
DUMP_RECORDS = [
    ("refent",   r"^# retail refent f=(\d+) slot=(\d+) ent=(\d+) reType=(\S+) renderfx=(\S+) "
                 r"hModel=(\S+) origin=(\S+) end=(\S+)"),
    ("poly",     r"^# retail poly f=(\d+) shader=(\S+) numverts=(\d+) renderfx=(\S+)"),
    ("trace",    r"^# retail trace f=(\d+) slot=(\d+) side=([gc]) start=(\S+) (?:mins=(\S+) maxs=(\S+) end=(\S+)|end=(\S+) mins=(\S+) maxs=(\S+))"),
    ("swipe",    r"^# retail swipe f=(\d+) slot=(\d+)"),
    ("es",       r"^# retail es f=(\d+) ent=(\d+) client=(-?\d+) etype=(\S+) eflags=(\S+)"),
    ("ps",       r"^# retail ps f=(\d+) cmdtime=(\d+) pmtype=(\d+) pmflags=(\S+)"),
    ("anim",     r"^# retail anim f=(\d+) svtime=(\d+) ent=(\d+) anim=(\S+) animidx=(\d+) pin=(\d) "
                 r"frame=(\S+) fidx=(\d+)"),
    ("ccmd",     r"^# retail ccmd f=(\d+) tiki=(\d+) anim=(\d+) name=(\S+) numframes=(\d+) "
                 r"hascmds=(\d+) numanims=(\d+)"),
    ("tag",      r"^# retail tag f=(\d+) slot=(\d+) tiki=(\d+)"),
    ("tagparent", r"^# retail tagparent f=(\d+) want=(-?\d+) found=(\d)"),
    ("snap",     r"^# retail snap f=(\d+) servertime=(\d+) nument=(\d+) psclient=(\d+)"),
    ("snapent",  r"^# retail snapent f=(\d+) ent=(\d+) client=(-?\d+) parent=(-?\d+) etype=(\S+)"),
    ("item",     r"^# retail item f=(\d+) slot=(\d+) ent=(\d+) model=(\d+) origin=(\S+) lerpOrigin=(\S+)"),
    ("spawn",    r"^# retail spawn f=(\d+) ms=\d+ map=(\S+) leveltime=(\d+) entstrbytes=(\d+)"),
    ("entstr",   r"^# retail entstr f=(\d+) map=(\S+) total=(\d+) part=(\d+) parts=(\d+)"),
    ("sound",    r"^# retail sound f=(\d+) ms=\d+ ent=(\d+) chan=(\d+) name=(\S+)"),
    ("soundlen", r"^# retail soundlen f=(\d+) ms=\d+ name=(\S+) len=(\S+)"),
    ("localize", r"^# retail localize f=(\d+) ms=\d+ in=(\S+) out=(\S+)"),
    ("cmd",      r"^# retail cmd f=(\d+) ms=\d+ slot=(\d+)"),
    ("setmodel", r"^# retail setmodel f=(\d+) ms=\d+ ent=(-?\d+) model=(\S+)"),
    ("world",    r"^# retail world f=(\d+) nument=(-?\d+) maxent=(-?\d+)"),
    ("menu",     r"^# retail menu f=(\d+) ms=\d+ slot=(\d+) name=(\S+) force=(\d)"),
    ("strres",   r"^# retail strres f=(\d+) ms=\d+ key=(\S+)"),
    ("dialog",   r"^# retail dialog f=(\d+) ms=\d+ slot=(\d+) name=(\S+)"),
    ("loadscreen", r"^# retail loadscreen f=(\d+) ms=\d+ slot=(\d+) text=(\S+)"),
]

# which retail record answers which open item, and what to compute from it
DUMP_PAIRINGS = [
    ("R131 ITEM 2 -- beam/fx convergence",
     ["refent", "trace", "poly", "tag", "tagparent"],
     "retail's beam START is refent origin (+68) and its END is refent `end` "
     "(+152, the field retail's own RB_SurfaceBeam reads); `trace side=c` is "
     "the GetOrientation clip.  If retail's end == the port's command-layer "
     "value the divergence is port rendering; if they differ it is upstream."),
    ("R131 ITEM 2b -- the first-person GATE (A1 rev2)",
     ["snap", "snapent"],
     "retail's gate is cg_entities[es.parent].clientNum == cg.snap->ps.clientNum. "
     "`snap` carries psclient (+252) once per frame and `snapent` carries each "
     "entity's client (+532) and parent (+176), so the gate is evaluable per "
     "entity per frame: gate = (client == psclient)."),
    ("R131 ITEM 3 -- missing bullets/grenades",
     ["ccmd", "anim", "es"],
     "`ccmd` is retail's own answer to \"how many frames does that animation "
     "have\" (read through retail's TIKI_Anim_NumFrames), so the port's stale "
     "state_nf is decidable against it.  `anim` is the change-detected SERVER "
     "lifecycle: a coalesced NewAnim-clear + SetFrame-pin in ONE server frame "
     "shows up as animidx changing while pin stays 1."),
    ("R131 ITEM 4 -- tag-child attach / tactical HUD",
     ["tag", "tagparent"],
     "`tagparent found=0` IS the whole-frame orphan drop (retail CG_Sprite "
     "@0x2ED88 / CG_ModelAnim @0x35810 early-out); `tag` is the resolved tag "
     "origin+axes and the parent refEntity's legsLive/torsoLive."),
    ("R131 ITEM A -- pickups clipping through the floor",
     ["item"],
     "retail's client-side item presentation: origin (the render origin), "
     "lerpOrigin (netorigin), and the full pos trajectory the client lerps "
     "from, with ground/solid/scale/alpha."),
    ("R131-B1 -- the prologue intro cluster",
     ["spawn", "entstr", "cmd", "localize", "sound", "soundlen", "dialog",
      "menu", "strres", "fsread", "loadscreen"],
     "`entstr` is retail's verbatim entity spawn set (the place to compare "
     "$picard byte for byte); `localize in=/out=` is retail's own 64-byte "
     "conversion result; `sound`/`soundlen` are the playdialog consequences "
     "(doesEntityExist's result, since the probe itself makes no contract "
     "call); `menu`/`strres`/`fsread` are the prologue menu + skip button; "
     "`loadscreen` + the |ms: anchors are the ordering."),
]


def dump_census(lines):
    """count the `# retail` records per family, and the first frame seen"""
    counts = {}
    frames = {}
    for fam, rx in DUMP_RECORDS:
        m = re.compile(rx)
        for l in lines:
            mm = m.match(l)
            if mm:
                counts[fam] = counts.get(fam, 0) + 1
                frames.setdefault(fam, mm.group(1))
    return counts, frames


def pairings(lines, gold_label):
    print("\n" + "=" * 78)
    print("RETAIL DUMP-WORKSTREAM PAIRINGS -- %s" % gold_label)
    print("=" * 78)
    counts, frames = dump_census(lines)
    if not counts:
        print("  no `# retail` records.  Re-capture with EF2_ORACLE_DUMPS set")
        print("  (see docs/oracle/doc/install.txt, section DUMPS).")
        return
    for item, fams, verdict in DUMP_PAIRINGS:
        print("\n-- %s" % item)
        for f in fams:
            n = counts.get(f, 0)
            if n:
                print("   %-10s %6d record(s), first at frame %s" % (f, n, frames[f]))
            else:
                print("   %-10s ABSENT (enable the family or the map never fires it)" % f)
        print("   -> %s" % verdict)


def normalize(line):
    """Strip volatile material: tty prefixes, timestamps, player names, numbers."""
    s = line.rstrip("\n")
    s = re.sub(r"^tty\]\s*", "", s)             # the launcher's tee prefix
    s = re.sub(r"^\[\d{2}:\d{2}:\d{2}\]\s*", "", s)
    s = re.sub(r"^\s*\d+\s+\d+\s+[FC]\s+[IE]\s+\d+\s+\S+", "<TRACE> ", s)  # oracle rows
    s = re.sub(r"\b(RedShirt|Operative93|Kleeya|Jurot|Erastas)\b", "<PLAYER>", s)
    s = re.sub(r"\d+\.\d+", "<F>", s)            # floats first
    s = re.sub(r"\b\d+\b", "<N>", s)
    s = s.strip()
    return s


def suppressed(norm):
    for pat in SUPPRESS:
        if re.search(pat, norm):
            return True
    return False


def read(path):
    try:
        with open(path, "r", errors="replace") as f:
            return f.readlines()
    except OSError as e:
        print("cannot read %s: %s" % (path, e))
        return []


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    port_path = sys.argv[1]
    gold_path = sys.argv[2] if len(sys.argv) > 2 else None

    port_raw = read(port_path)

    # ---- 1. probe walk (the decisive output) ------------------------------
    print("=" * 78)
    print("R120 PROBE WALK -- %s" % port_path)
    print("=" * 78)
    norm_all = [normalize(l) for l in port_raw]
    probe_lines = [l for l in port_raw if "[R120:" in l]
    print("probe lines found: %d" % len(probe_lines))
    if not probe_lines:
        print("  -> NO [R120:*] lines.  Either qa_r120.cfg did not exec (look for")
        print("     '[R120:cfg] exec=1') or this binary predates the R120 probes.")
    for chain, links in PROBE_PROTOCOL.items():
        print("\n-- %s" % chain)
        first_missing = None
        for link_id, rx, proves in links:
            hits = [l.rstrip() for l in probe_lines if re.search(rx, l)]
            if not hits and first_missing is None:
                first_missing = link_id
                print("   %-15s MISSING   (would prove: %s)" % (link_id, proves))
            elif not hits:
                print("   %-15s missing   (proves: %s)" % (link_id, proves))
            else:
                fails = [h for h in hits if any(m in h for m, _ in FAILURE_MARKERS)]
                verdict = "ok  (%d lines)" % len(hits)
                note = ""
                if fails:
                    expl = next(expl for m, expl in FAILURE_MARKERS if m in fails[0])
                    note = "  <-- FAILURE: " + expl
                print("   %-15s %s%s" % (link_id, verdict, note))
                if hits:
                    print("        last: %s" % hits[-1].strip()[:150])
        if first_missing:
            print("   ==> FIRST SILENT LINK: %s" % first_missing)

    # ---- 2. failure markers ----------------------------------------------
    print("\n" + "=" * 78)
    print("FAILURE MARKERS")
    print("=" * 78)
    any_fail = False
    for l in probe_lines:
        for m, expl in FAILURE_MARKERS:
            if m in l:
                any_fail = True
                print("  %s\n      -> %s" % (l.strip()[:160], expl))
    if not any_fail:
        print("  none")

    # ---- 3. ordered class fork against the golden -------------------------
    print("\n" + "=" * 78)
    print("ORDERED CLASS FORK")
    print("=" * 78)
    port_kept = []
    for l in port_raw:
        n = normalize(l)
        if suppressed(n):
            continue
        if n:
            port_kept.append(n)

    if not gold_path:
        print("no retail golden given -- class census only (%d live port lines)"
              % len(port_kept))
        for label, rx in PORT_CLASSES:
            hits = [n for n in port_kept if re.search(rx, n)]
            print("   %-13s %s" % (label, "present" if hits else "ABSENT"))
        return

    gold_raw = read(gold_path)
    gold_kept = []
    for l in gold_raw:
        n = normalize(l)
        if suppressed(n):
            continue
        if n:
            gold_kept.append(n)

    # R131: the retail dump-workstream pairings (a `# retail` census of the golden)
    pairings(gold_raw, gold_path)

    # The oracle is an API-call trace, not a console log, so only the classes
    # that exist on BOTH sides are comparable.  Report the first class the port
    # lacks, then the first class retail lacks.
    port_absent = [lb for lb, rx in PORT_CLASSES
                   if not any(re.search(rx, n) for n in port_kept)]
    gold_absent = [lb for lb, rx in PORT_CLASSES
                   if not any(re.search(rx, n) for n in gold_kept)]
    print("  port lines kept: %d   retail lines kept: %d" % (len(port_kept), len(gold_kept)))
    print("  classes the PORT is missing : %s" % (port_absent or "none"))
    print("  classes RETAIL is missing   : %s" % (gold_absent or "none"))
    fork = [lb for lb in port_absent if lb not in gold_absent]
    print("  FIRST STRUCTURAL FORK       : %s" % (fork[0] if fork else
          "none (all shared classes present on both sides)"))
    # Retail-only evidence the port cannot have: the NPC names.
    for name in ("Operative93", "Kleeya", "Jurot", "Erastas"):
        in_gold = any(name in l for l in gold_raw)
        in_port = any(name in l for l in port_raw)
        print("  bot %-13s retail=%-5s port=%s" % (name, in_gold, in_port))
    print("\n  NOTE: retail.F is an fgame API-call trace and retail.C a cgame trace,")
    print("  NOT a console log -- text equality with the port log is meaningless past")
    print("  the class level.  Use the probe walk above for link-level verdicts.")


if __name__ == "__main__":
    main()
