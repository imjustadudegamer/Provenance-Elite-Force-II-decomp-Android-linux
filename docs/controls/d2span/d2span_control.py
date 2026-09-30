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

def stream(menu_like=True):
    ev = []
    ev.append(("win", "A"))
    ev.append(("color", "c0"))
    ev.append(("quad", 6))
    if menu_like:
        ev.append(("win", "B"))          # the next widget's own window
        ev.append(("color", "c1"))       # and its own Draw_SetColor
        ev.append(("quad", 6))
        ev.append(("scn", "B/2"))        # its own scissor
        ev.append(("color", "c2"))
        ev.append(("quad", 6))
        ev.append(("win", "C"))
        ev.append(("quad", 6))
        ev.append(("shader", "s2"))      # a different material ends the batch
        ev.append(("quad", 6))
        ev.append(("win", "D"))
        ev.append(("quad", 6))
    else:
        ev.append(("quad", 6))
        ev.append(("quad", 6))
    return ev


def old_model(ev):
    """Every window or scissor change ends the surface: what the walk did."""
    state = None
    queue = []
    draws = []                    # (state, [quad id])
    recorded = {}                 # quad id -> state at record time
    color = None                  # backEnd.color2D, per widget in this sequencing
    colorOf = {}                  # quad id -> the colour it was drawn with
    qid = 0

    for kind, arg in ev:
        if kind == "color":
            color = arg
        elif kind in ("win", "scn"):
            if queue:
                draws.append((state, queue))
                queue = []
            state = arg if kind == "win" else state + "|" + arg
        elif kind == "shader":
            if queue:
                draws.append((state, queue))
                queue = []
        elif kind == "quad":
            recorded[qid] = state
            colorOf[qid] = color
            queue.append(qid)
            qid += 1

    if queue:
        draws.append((state, queue))
    return {"draws": draws, "recorded": recorded,
            "color": colorOf,
            "ends": len(draws), "spans": sum(1 for st, q in draws if q)}


def new_model(ev, mutant=0):
    """One surface per shader; a state change inside it is a span."""
    state = None
    spans = []                    # (firstIndex, state)
    queue = []                    # (quad id, first index in the batch)
    batch_indexes = 0
    drawn = []
    recorded = {}
    color = None                  # the walk's live backEnd.color2D
    colorOf = {}                  # quad id -> the colour recorded with it
    drawnColor = {}               # quad id -> the colour it is drawn with
    ends = 0
    qid = 0

    def end_surface():
        """RB_EndSurface: one draw per span, or one draw when under two."""
        nonlocal spans, queue, batch_indexes, ends
        if queue:
            ends += 1
        if not queue:
            spans, batch_indexes = [], 0
            return
        if mutant == 6 or len(spans) < 2:
            # planted 6: the whole batch as one draw, which is what the old
            # sequencing did per window and so cannot carry two states
            drawn.append((state, [q for q, fi in queue]))
            for q, fi in queue:
                drawnColor[q] = color if mutant == 7 else colorOf[q]
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
        spans, queue, batch_indexes = [], [], 0

    for kind, arg in ev:
        if kind == "color":
            color = arg                # RC_2D_COLOR, live during the walk
        elif kind in ("win", "scn"):
            # RB_2D_SpanBreak(): the queued quads close under the state in force
            if queue and batch_indexes > 0 and len(spans) == 0:
                spans.append((0, "NEW" if mutant == 1 else state))
            # the walk applies the new state here
            state = arg if kind == "win" else state + "|" + arg
            # RB_2D_SpanNote(): the quads that follow belong to the new state
            if (queue or batch_indexes > 0) and mutant != 3:
                if spans and spans[-1][0] == batch_indexes:
                    spans.pop()    # that segment got no quads
                spans.append((batch_indexes, state))
        elif kind == "shader":
            end_surface()
        elif kind == "quad":
            recorded[qid] = state
            colorOf[qid] = color
            queue.append((qid, batch_indexes))
            batch_indexes += arg
            qid += 1

    end_surface()
    return {"draws": drawn, "recorded": recorded, "color": colorOf,
            "drawnColor": drawnColor,
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


def main():
    mutant = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    cmds_path = sys.argv[2] if len(sys.argv) > 2 else CMDS
    draw_path = sys.argv[3] if len(sys.argv) > 3 else DRAW
    batch_path = sys.argv[4] if len(sys.argv) > 4 else BATCH
    shade_path = sys.argv[5] if len(sys.argv) > 5 else os.path.join(VK, "tr_shade.c")

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

    span_get = body(batch, "void RB_2D_SpanGet(")
    check("tess.numIndexes" in span_get,
          "RB_2D_SpanGet does not run the last span to the batch's end")

    # --- the model --------------------------------------------------------
    run_model(mutant)

    print("d2span control: %d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
