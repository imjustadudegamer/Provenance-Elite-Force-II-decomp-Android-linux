#!/usr/bin/env python3
"""
d2span_control.py -- the 2D batch's state spans, proven offline.

A UI widget sets its own window -- and usually its own scissor -- before it
draws, so a menu's 2D run used to end the surface at every widget: RB_2D_Flush
on each RC_2D_SET_WINDOW and RC_2D_SCISSOR.  What actually changed at a widget
boundary is the ortho push constant and the viewport and scissor, all dynamic
state, so the batch now keeps one span per state and re-applies each span's copy
before that span's own indexed draw.

This control asserts the two things that make that a pure win rather than a
render change:

  1. the source still sequences it the safe way -- the immediate run closes at a
     window change, the pending quads' state closes as a span BEFORE the new
     state is applied, the new state opens as the next span AFTER it, the batch
     still ends where it must (a shader change, the pre-3D flush, a purge), and
     both of the stage pass's draw sites go through the span draw;
  2. the model: over generated request streams, every quad is drawn exactly once
     and with exactly the state it was recorded under, in the order it was
     recorded -- the same per-quad state and the same draw order the old
     flush-per-window sequencing produced, with strictly fewer surface ends for
     a stream whose widgets share a shader;
  3. the draw colour, which is the input a span cannot carry in the command
     buffer: the two generators that read it (CGEN_GLOBAL_COLOR, AGEN_GLOBAL_ALPHA)
     must take it from the quad's own vertices, because a batch now holds quads
     from many widgets.  Every 2D quad bakes the colour in force at its append
     (RB_Color4bv(backEnd.color2D) in tr_draw.c), so the vertex colour IS the
     widget's colour; reading backEnd.color2D at draw time gives the batch the
     last widget's, which is the missing and mis-tinted menu 2D.  The model
     carries a colour per quad and requires that the colour a quad is drawn with
     is the colour it was recorded with.

Planted faults (argv[1]) break each of those in turn; a source fault is planted
by run.sh on a copy of the shipping file, which is passed as argv[2:] in the
order cmds, draw, batch, and the colour faults are planted in tr_shade.c, which
the control reads from the same tree.

Build/run: docs/controls/d2span/run.sh  (never launches the game)
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
WT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
VK = os.path.join(WT, "app", "src", "main", "cpp", "code", "renderervulkan")

CMDS = os.path.join(VK, "tr_cmds.c")
DRAW = os.path.join(VK, "tr_backend.c")
BATCH = os.path.join(VK, "tr_draw.c")

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


def case_body(text, case):
    """The source of one switch case, from its label to the next case label."""
    i = text.find("case %s:" % case)
    if i < 0:
        return ""
    j = text.find("\n\t\tcase ", i + 1)
    return text[i:j] if j > 0 else text[i:]


def body(text, header):
    i = text.find(header)
    if i < 0:
        return ""
    j = text.find("\n}\n", i)
    return text[i:j + 3] if j > 0 else text[i:]


# ---- the model --------------------------------------------------------------
#
# A stream is a list of events.  ("win", key) and ("scn", key) change the state;
# ("quad", 6) queues one quad; ("shader", name) changes the surface's shader,
# which ends the batch in both sequencings.

# The scissor rects retail's own 2D draw stream carries, read out of
# personal/evidence/traces/retail-menu-solomatch-final.trace (the glScissor of
# the main menu's widget pass, bottom-left client coords in retail's 640x480
# space): the tooltip, the four top buttons, Exit, the credits widget and the
# two version labels.  The model's states are these, not a shape the port
# invented, so what it proves is that a span draws a quad under the rect THE
# TRACE gives that widget.
TRACE_SCN = [
    (49, 51, 320, 26), (44, 393, 138, 32), (182, 393, 138, 32),
    (320, 393, 138, 32), (458, 393, 138, 32), (435, 20, 96, 32),
    (371, 49, 256, 32), (424, 53, 123, 26), (20, 9, 25, 16),
    (40, 9, 40, 16),
]


def stream(menu_like=True):
    ev = []
    ev.append(("win", "A"))
    ev.append(("color", "c0"))
    ev.append(("origin", "A"))
    ev.append(("quad", 6))
    if menu_like:
        ev.append(("win", "B"))          # the next widget's own window
        ev.append(("color", "c1"))       # and its own Draw_SetColor
        ev.append(("origin", "B"))       # and its own placement (Rend_Set2DOrigin)
        ev.append(("quad", 6))
        ev.append(("scn", "B/2"))        # its own scissor
        ev.append(("color", "c2"))
        ev.append(("origin", "C"))
        ev.append(("glyph", 6))          # a text run: one quad per glyph, same span
        ev.append(("quad", 6))
        ev.append(("win", "C"))
        ev.append(("quad", 6))
        ev.append(("shader", "s2"))      # a different material ends the batch
        ev.append(("win", "D"))          # the next widget sets its state first,
        ev.append(("origin", "D"))       # and only then draws: the surface ends
        ev.append(("quad", 6))           # at that draw, inside the new state
        ev.append(("scn", "D/2"))
        ev.append(("quad", 6))
        # the trace's own rects, one widget each, consecutive materials: the
        # menu's six background tiles have exactly this shape, and every one of
        # them must be drawn under the rect the trace carries for it.
        for i, scn in enumerate(TRACE_SCN):
            ev.append(("win", "T%d" % i))
            ev.append(("scn", scn))
            ev.append(("shader", "t%d" % i))
            ev.append(("quad", 6))
    else:
        ev.append(("quad", 6))
        ev.append(("origin", "A2"))
        ev.append(("glyph", 6))          # a HUD label in the same surface
        ev.append(("quad", 6))
        ev.append(("color", "c1"))
        ev.append(("origin", "A3"))
        ev.append(("glyph", 12))
        ev.append(("color", "c0"))
    return ev


def old_model(ev):
    """Every window or scissor change ends the surface: what the walk did."""
    state = None
    queue = []
    draws = []                    # (state, [quad id])
    recorded = {}                 # quad id -> state at record time
    color = None                  # backEnd.color2D, per widget in this sequencing
    colorOf = {}                  # quad id -> the colour it was drawn with
    origin = None                 # the window's ortho carried the placement
    originOf = {}                 # quad id -> the placement baked into it
    glyphs = []                   # the quads that are glyphs of a text run
    qid = 0

    for kind, arg in ev:
        if kind == "color":
            color = arg
        elif kind == "origin":
            origin = arg
        elif kind in ("win", "scn"):
            if queue:
                draws.append((state, queue))
                queue = []
            state = arg if kind == "win" else (state, arg)
        elif kind == "shader":
            if queue:
                draws.append((state, queue))
                queue = []
        elif kind in ("quad", "glyph"):
            recorded[qid] = state
            colorOf[qid] = color
            originOf[qid] = origin
            queue.append(qid)
            if kind == "glyph":
                glyphs.append(qid)
            qid += 1

    if queue:
        draws.append((state, queue))
    return {"draws": draws, "recorded": recorded,
            "color": colorOf, "origin": originOf, "glyphs": glyphs,
            "ends": len(draws), "spans": sum(1 for st, q in draws if q)}


def new_model(ev, mutant=0):
    """One surface per shader; a state change inside it is a span, and the span a
    quad belongs to is the state the quad is AUTHORED under, recorded at the quad
    (RB_2D_SpanQuad) from the walk's 2D mirror.

    The mirror is the one VK_Draw_2DStateGet reads.  A span replay runs while the
    run is inside a later widget, so the replay must NOT write that mirror (the
    vk_2dReplaying guard, vk_draw.c): if it does, the next quad's snapshot reads
    back the state of the span that was just replayed instead of the state in
    force, and the quad draws under another widget's clip."""
    state = None                  # the state in force
    live = None                   # the mirror a snapshot reads
    spans = []                    # (firstIndex, state)
    queue = []                    # (quad id, first index in the batch)
    batch_indexes = 0
    drawn = []
    recorded = {}
    color = None                  # the walk's live backEnd.color2D
    colorOf = {}                  # quad id -> the colour recorded with it
    origin = None                 # the walk's live placement, added at append
    originOf = {}                 # quad id -> the placement baked into it
    drawnOrigin = {}              # quad id -> the placement it is drawn with
    drawnColor = {}               # quad id -> the colour it is drawn with
    glyphs = []                   # the quads that are glyphs of a text run
    ends = 0
    qid = 0
    surface_shader = None
    pending_shader = None

    # planted 1: the old shape, which guessed a quad's state at the NEXT state
    # command instead of recording it at the quad.
    state_at = []
    running = None
    for kind, arg in ev:
        if kind == "win":
            running = arg
        elif kind == "scn":
            running = arg if running is None else (running, arg)
        state_at.append(running)
    after = [None] * len(ev)
    nxt = None
    for i in range(len(ev) - 1, -1, -1):
        after[i] = nxt
        if ev[i][0] in ("win", "scn"):
            nxt = state_at[i]

    def end_surface():
        """RB_EndSurface / RB_StageDrawSpan: one draw per span, or one draw when
        under two, which is the short path that cannot name a state."""
        nonlocal spans, queue, batch_indexes, ends, live
        if queue:
            ends += 1
        if not queue:
            spans, batch_indexes = [], 0
            return
        if mutant == 6 or len(spans) == 0:
            # planted 6, or the unnameable case: the whole batch as one draw
            # under whatever state is in force at the flush.  A batch of ONE span
            # is NOT this case -- RB_2D_SpanCount reports it as two so the draw
            # still applies the span's own state.
            drawn.append((state, [q for q, fi in queue]))
            for q, fi in queue:
                drawnColor[q] = color if mutant == 7 else colorOf[q]
                drawnOrigin[q] = origin if mutant == 12 else originOf[q]
            if mutant == 16:
                live = state
        else:
            for i, (first, st) in enumerate(spans):
                end = spans[i + 1][0] if i + 1 < len(spans) else batch_indexes
                if mutant == 2:
                    end = first if i + 1 == len(spans) else end  # planted: tail
                part = [q for q, fi in queue if first <= fi < end]
                if part:
                    drawn.append((st, part))
                    for q in part:
                        drawnColor[q] = color if mutant == 7 else colorOf[q]
                        drawnOrigin[q] = origin if mutant == 12 else originOf[q]
                    if mutant == 16:
                        live = st      # planted: the replay wrote the mirror
        spans, queue, batch_indexes = [], [], 0

    for idx, (kind, arg) in enumerate(ev):
        if kind == "color":
            color = arg                # RC_2D_COLOR, live during the walk
        elif kind == "origin":
            origin = arg               # RC_2D_ORIGIN, live during the walk
        elif kind in ("win", "scn"):
            # VK_Draw_Set2D / VK_Draw_SetScissor apply the state and write the
            # mirror; planted 3 is the mirror left behind by the change.
            state = arg if kind == "win" else (state, arg)
            if mutant != 3:
                live = state
        elif kind == "shader":
            # RB_2D_Batch only ends the surface when the NEXT quad arrives with a
            # different material, which is why a state change can sit between the
            # last quad of one surface and the first quad of the next.
            pending_shader = arg
        elif kind in ("quad", "glyph"):
            if pending_shader is not None and pending_shader != surface_shader:
                end_surface()
                surface_shader = pending_shader
            pending_shader = None

            recorded[qid] = state
            colorOf[qid] = color
            originOf[qid] = origin

            # RB_2D_SpanQuad: the state this quad is authored under, read here
            # and not guessed at the next change.
            snap = after[idx] if mutant == 1 and after[idx] is not None else live
            if not spans:
                spans.append((batch_indexes, snap))
            elif spans[-1][1] != snap:
                spans.append((batch_indexes, snap))

            queue.append((qid, batch_indexes))
            batch_indexes += arg
            if kind == "glyph":
                glyphs.append(qid)
            qid += 1

    end_surface()
    return {"draws": drawn, "recorded": recorded, "color": colorOf,
            "origin": originOf, "drawnOrigin": drawnOrigin,
            "drawnColor": drawnColor, "glyphs": glyphs,
            "ends": ends, "spans": sum(1 for st, q in drawn if q)}


def order(draws):
    return [q for st, quads in draws for q in quads]


def run_model(mutant):
    for menu_like in (True, False):
        ev = stream(menu_like)
        old = old_model(ev)
        new = new_model(ev, mutant)
        tag = "menu" if menu_like else "plain"

        od, nd = order(old["draws"]), order(new["draws"])

        check(od == nd,
              "%s: the spans drew a different set or order of quads (%r vs %r)"
              % (tag, od, nd))
        for q in sorted(set(old["recorded"]) | set(new["recorded"])):
            check(old["recorded"].get(q) == new["recorded"].get(q),
                  "%s: quad %d was recorded under %r, now %r"
                  % (tag, q, old["recorded"].get(q), new["recorded"].get(q)))
        for st, quads in new["draws"]:
            for q in quads:
                check(st == new["recorded"][q],
                      "%s: quad %d is drawn under %r, recorded under %r"
                      % (tag, q, st, new["recorded"][q]))

        if menu_like:
            check(new["ends"] < old["ends"],
                  "%s: the spans did not reduce the surface ends (%d -> %d)"
                  % (tag, old["ends"], new["ends"]))
        else:
            check(new["ends"] <= old["ends"],
                  "%s: a one-window run ended more surfaces (%d -> %d)"
                  % (tag, old["ends"], new["ends"]))
            check(new["spans"] >= new["ends"],
              "%s: more surfaces than draws, which cannot happen" % tag)

        # the draw colour: every quad must be drawn with the colour it was
        # recorded with, which is the quad's own and not the last widget's
        for q in sorted(old["color"]):
            check(old["color"].get(q) == new["color"].get(q),
                  "%s: quad %d was recorded under colour %r, now %r"
                  % (tag, q, old["color"].get(q), new["color"].get(q)))
        for q in sorted(new["drawnColor"]):
            check(new["drawnColor"][q] == new["color"][q],
                  "%s: quad %d is drawn with colour %r, recorded with %r"
                  % (tag, q, new["drawnColor"][q], new["color"][q]))

        # a text run is exactly the same shape: one quad per glyph, each placed
        # at append, so a span holding two widgets' runs draws each glyph where
        # it was recorded
        check(len(new["glyphs"]) > 0,
              "%s: the stream carried no glyph quads, so the font path is not "
              "exercised" % tag)
        check(sorted(old["glyphs"]) == sorted(new["glyphs"]),
              "%s: the spans drew a different set of glyph quads (%r vs %r)"
              % (tag, sorted(old["glyphs"]), sorted(new["glyphs"])))
        for q in new["glyphs"]:
            check(q in nd,
                  "%s: glyph quad %d is never drawn" % (tag, q))
            check(new["drawnOrigin"].get(q) == new["origin"].get(q),
                  "%s: glyph quad %d is drawn at placement %r, recorded at %r"
                  % (tag, q, new["drawnOrigin"].get(q), new["origin"].get(q)))

        # the trace's own scissor rects: every widget is drawn under the rect the
        # trace carries for it, and every rect the trace carries is drawn
        if menu_like:
            drawn_scn = [st[1] for st, quads in new["draws"] if quads
                         and isinstance(st, tuple) and len(st) == 2
                         and st[1] in TRACE_SCN]
            check(sorted(drawn_scn) == sorted(TRACE_SCN),
                  "%s: the spans drew %d of the trace's %d scissor rects (%r)"
                  % (tag, len(drawn_scn), len(TRACE_SCN), sorted(set(drawn_scn))))

        # the placement is baked into the quad's own vertices at append, so a span
        # holding several widgets draws each where it was recorded
        for q in sorted(old["origin"]):
            check(old["origin"].get(q) == new["origin"].get(q),
                  "%s: quad %d was recorded at placement %r, now %r"
                  % (tag, q, old["origin"].get(q), new["origin"].get(q)))
        for q in sorted(new["drawnOrigin"]):
            check(new["drawnOrigin"][q] == new["origin"][q],
                  "%s: quad %d is drawn at placement %r, recorded at %r"
                  % (tag, q, new["drawnOrigin"][q], new["origin"][q]))


def main():
    mutant = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    cmds_path = sys.argv[2] if len(sys.argv) > 2 else CMDS
    draw_path = sys.argv[3] if len(sys.argv) > 3 else DRAW
    batch_path = sys.argv[4] if len(sys.argv) > 4 else BATCH
    shade_path = sys.argv[5] if len(sys.argv) > 5 else os.path.join(VK, "tr_shade.c")
    font_path = sys.argv[6] if len(sys.argv) > 6 else os.path.join(VK, "tr_font.cp" + "p")
    util_path = sys.argv[7] if len(sys.argv) > 7 else os.path.join(VK, "tr_util.cpp")
    imm_path = sys.argv[8] if len(sys.argv) > 8 else None
    vk_path = sys.argv[9] if len(sys.argv) > 9 else os.path.join(VK, "vk_draw.c")

    cmds = read(cmds_path)
    draw = read(draw_path)
    batch = read(batch_path)

    # --- the walk's window case ------------------------------------------
    win = case_body(cmds, "RC_2D_SET_WINDOW")
    scn = case_body(cmds, "RC_2D_SCISSOR")

    for name, case in (("RC_2D_SET_WINDOW", win), ("RC_2D_SCISSOR", scn)):
        check("RB_2D_FlushImmediate();" in case,
              "%s does not close the immediate run, so a box or line queued under "
              "the old state draws under the new one" % name)
        i_break = case.find("RB_2D_SpanBreak()")
        i_apply = case.find("VK_Draw_Set2D(" if name == "RC_2D_SET_WINDOW" else "VK_Draw_SetScissor(")
        i_note = case.find("RB_2D_SpanNote()")
        check(i_break >= 0 and i_apply > i_break and i_note > i_apply,
              "%s does not break the span before applying the new state and note "
              "it after (break=%d apply=%d note=%d)" % (name, i_break, i_apply, i_note))

        # the only remaining flush is the fallback, which runs before the apply
        stripped = re.sub(r"if\s*\(\s*!\s*RB_2D_Span(Break|Note)\(\)\s*\)\s*\{\s*RB_2D_Flush\(\);\s*\}",
                          "", case)
        check("RB_2D_Flush();" not in stripped,
              "%s still ends the 2D batch unconditionally" % name)

    # --- the batch still ends where it must -------------------------------
    # The 3D guard sits before the case: the 2D batch is recorded when it
    # flushes, so a 3D command after one in the list would overtake it.
    guard = cmds.find("if ( op == RC_DRAW_SURFS || op == RC_SPRITE_SURFS )")
    segment = cmds[guard:guard + 700] if guard >= 0 else ""
    check(guard >= 0 and "RB_2D_Flush();" in segment,
          "the pre-3D flush is gone, so a 3D command overtakes the pending 2D "
          "(the Controls stardome lands under its own full-screen fill)")

    batch_open = body(batch, "static void RB_2D_Batch(")
    check("RB_2D_Flush();" in batch_open,
          "RB_2D_Batch no longer ends the batch on a shader change")

    flush = body(batch, "void RB_2D_Flush( void )")
    check("RB_2D_FlushImmediate();" in flush,
          "RB_2D_Flush no longer closes the immediate run, so a purge or a frame "
          "end leaves queued boxes undrawn")
    check("RB_2D_SpanReset();" in flush,
          "RB_2D_Flush does not drop the spans, so the next batch redraws them")

    # --- the spans are what the stage pass draws --------------------------
    stage = body(draw, "static qboolean RB_StagePass(")
    check(stage.count("RB_StageDrawSpan(") == 2,
          "the stage pass does not draw both of its paths through the spans (%d)"
          % stage.count("RB_StageDrawSpan("))
    check("RB_2D_SpanReset();" in body(draw, "void RB_FrameOpen("),
          "RB_FrameOpen does not drop the spans, so one frame's spans can outlive it")

    # --- the draw colour rides on the quad --------------------------------
    shade = read(shade_path)
    resolve = body(shade, "qboolean VK_StageResolve2DColor(")
    colors = body(shade, "static void ComputeColors(")
    if mutant == 8:
        resolve = resolve.replace("s_2DColorFromVertex = qtrue;", "")
    if mutant == 9:
        colors = colors.replace("if ( s_2DColorFromVertex ) {", "if ( 0 ) {")
    check("s_2DColorFromVertex = qtrue;" in resolve,
          "VK_StageResolve2DColor does not take the 2D draw colour from the quad, "
          "so a batch that spans widgets draws every quad with the last widget's")
    for gen in ("CGEN_GLOBAL_COLOR", "AGEN_GLOBAL_ALPHA"):
        i = colors.find("case %s:" % gen)
        branch = colors[i:i + 400] if i >= 0 else ""
        check("s_2DColorFromVertex" in branch,
              "ComputeColors' %s does not read the quad's own colour in 2D" % gen)
    check("backEnd.color2D" in colors,
          "the 3D path lost its draw colour (backEnd.color2D)")

    # --- the placement rides on the vertices, not in the span state --------
    #
    # One choke point per lane (RB_2DPlaceVertex/range in tr_util.cpp) adds the
    # widget's offset, and EVERY 2D writer ends with it: the surface batch, the
    # immediate stream and the font/glyph runs.  The placement is baked at append,
    # so a span holding quads and glyphs from many widgets draws each where it was
    # recorded -- reading a live origin at draw time would give the batch the last
    # widget's, which is the missing and mis-placed menu 2D.
    util = read(util_path)
    backend = read(os.path.join(VK, "tr_backend.c"))
    choke = body(util, "void RB_2DPlaceVertex(")
    if mutant == 13:
        choke = choke.replace("xyz[0] += vk_2dOrigin[0];", "")
        choke = choke.replace("xyz[1] += vk_2dOrigin[1];", "")
    check("xyz[0] += vk_2dOrigin[0];" in choke and "xyz[1] += vk_2dOrigin[1];" in choke
          and "backEnd.in2D" in choke,
          "the single placement site does not add the widget's offset (both axes, "
          "in2D-guarded), so a span holding several widgets cannot be right")
    vert = body(util, "void RB_Vertex3f(")
    check("RB_2DPlaceVertex(" in vert,
          "the surface batch's 2D vertex writer does not go through the placement "
          "site, so a span holding several widgets cannot be right")
    imm = body(backend, "void RB_ImmVertex2f(")
    imm = body(read(imm_path), "void RB_ImmVertex2f(") if imm_path else imm
    check("RB_2DPlaceVertex(" in imm,
          "the immediate stream's 2D vertex writer does not go through the "
          "placement site")

    # the font/glyph runs: one quad per glyph, each placed, and placed AFTER the
    # glyph's own width/height scale (the placement is an offset, not a scaled
    # quantity)
    font = read(font_path)
    for header in ("static void R_FontRunWrite(", "void RB_FontRunDraw(",
                   "void R_DrawString_sgl("):
        b = body(font, header)
        if not b:
            continue
        if mutant == 15:
            # planted: the placement moves above the glyph scale
            b = b.replace("RB_2DPlaceRange( tess.numVertexes, 4 );", "", 1)
            i = b.find("tess.xyz[tess.numVertexes][0] =")
            b = b[:i] + "RB_2DPlaceRange( tess.numVertexes, 4 );\n" + b[i:]
        check("RB_2DPlaceRange(" in b,
              "the glyph run %s writes its quads without the placement, so a span's "
              "glyphs land in widget-local coordinates and the widget's scissor "
              "clips the text away" % header)
        i_scale = max([b.rfind("*= s_fontRun"), b.rfind("*= fWidthScale"),
                       b.rfind("*= cmd->widthScale")])
        i_place = b.rfind("RB_2DPlaceRange(")
        check(i_scale > 0 and i_place > i_scale,
              "the glyph run %s places its quads before the glyph width/height "
              "scale, so the widget's offset is scaled too" % header)
    check("origin" not in body(batch, "void RB_2D_SpanGet(").lower(),
          "the span state carries a placement, so a span would move the quads a "
          "later widget recorded")
    check("origin" not in body(draw, "static void RB_StageDrawSpan(").lower(),
          "the span draw applies a placement, which belongs on the vertices")

    span_get = body(batch, "void RB_2D_SpanGet(")
    check("tess.numIndexes" in span_get,
          "RB_2D_SpanGet does not run the last span to the batch's end")

    # --- the span is the state the quad is AUTHORED under ------------------
    #
    # RB_2D_SpanQuad records it at the writer, from the walk's 2D mirror, so the
    # snapshot cannot be taken later, when a following widget's state (or a
    # replayed span) has already changed what the mirror says.
    bq = body(batch, "static void RB_2D_BatchQuad(")
    if mutant == 18:
        bq = bq.replace("RB_2D_SpanQuad();", "")
    i_snap = bq.find("RB_2D_SpanQuad();")
    i_verts = bq.find("b = tess.numVertexes;")
    check(i_snap >= 0 and i_verts > i_snap,
          "RB_2D_BatchQuad does not record the quad's own state before it writes "
          "the vertices (snap=%d verts=%d), so the span is guessed from a state a "
          "replay can already have written" % (i_snap, i_verts))

    span_count = body(batch, "int RB_2D_SpanCount( void )")
    check("s_2dSpanCount == 1" in span_count and "2 :" in span_count,
          "RB_2D_SpanCount can report a single span, which sends the batch down "
          "the draw path that uses whatever state is in force at the flush "
          "instead of the state the span was authored under")

    # --- the replay does not become the state it restores -------------------
    #
    # VK_Draw_2DStateApply re-issues a span's state while the run is inside a
    # later widget.  The mirrors it writes are what VK_Draw_2DStateGet reads, so
    # a replay that writes them makes the next quad's snapshot read the replayed
    # span's scissor -- a widget drawn under another widget's clip.
    vk = read(vk_path)
    apply = body(vk, "void VK_Draw_2DStateApply(")
    scissor = body(vk, "void VK_Draw_SetScissor(")
    if mutant == 17:
        apply = apply.replace("vk_2dReplaying = qtrue;", "")
        apply = apply.replace("vk_2dReplaying = qfalse;", "")
    check("vk_2dReplaying = qtrue;" in apply and "vk_2dReplaying = qfalse;" in apply,
          "VK_Draw_2DStateApply does not bracket the state it restores with the "
          "replay guard, so restoring a span becomes the state the next snapshot "
          "reads back")
    check("vk_2dReplaying" in scissor and "if ( !vk_2dReplaying )" in scissor,
          "VK_Draw_SetScissor writes the live 2D mirror during a replay, so the "
          "next quad's span snapshot reads a replayed span's scissor")

    # --- the model --------------------------------------------------------
    run_model(mutant)

    print("d2span control: %d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
