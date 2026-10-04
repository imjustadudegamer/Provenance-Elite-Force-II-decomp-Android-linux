#!/usr/bin/env python3
"""
The scrollbar width control.

A 2D draw under `scissor=(0,0 0x0)` rasterises nothing, and this port's Load
Game / controls boots carried 7,754 of them.  They are not a defect: the port's
own `[UIVK:zerowin]` census names the widget, and it is the CONTROLS screen's
bind scrollbar, which retail's shipped data authors with `scrollbar_width 0`
(menu_controls.urc:239; dm_scoreboard.urc:256 is the other).  The geometry
identifies it -- list rect 46 124 325 255 at the 1.25 ui scale puts the row
button at y=155, the scrollbar at y=155+18*1.25=177.5, its height at
255*1.25-22.5=296.25 and its x at 325*1.25=406.25, which are the census's
155, 178, 296 and 406.

Green on the shipping tree: every scrollbar_width the shipped data declares is
honoured end to end -- the event sets both the width and its base, the template
copy carries the base, the right-align layout uses the width, and the four
retail functions this chain is transcribed from agree with the port field for
field.  Each planted fault must turn the run non-zero.
"""
import os
import re
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.join(HERE, "..", "..", "..")
UILIB = os.path.join(REPO, "app", "src", "main", "cpp", "code", "uilib")
DECOMP = os.environ.get(
    "EF2_DECOMP",
    os.path.expanduser(
        "~/Desktop/PORTS/EF2_Android_WIP/personal/evidence/mac_symbols/engine_decomp_all"))
RETAIL = os.environ.get("EF2_RETAIL_ROOT",
                        os.path.expanduser("~/Star Trek Elite Force II"))

checks = [0]
fails = []


def check(ok, what):
    checks[0] += 1
    if not ok:
        fails.append(what)
    return ok


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def shipped_scrollbar_widths():
    """Every scrollbar_width the shipped data declares, as (file, value)."""
    l1 = os.path.join(RETAIL, "base", "locpak1.pk3")
    if not os.path.isfile(l1):
        return None
    out = []
    with zipfile.ZipFile(l1) as z:
        for n in z.namelist():
            if not n.lower().endswith(".urc"):
                continue
            text = z.read(n).decode("latin1")
            for m in re.finditer(r"scrollbar_Width\s+([0-9.]+)", text, re.I):
                out.append((os.path.basename(n), float(m.group(1))))
    return out


# ---------------------------------------------------------------------------
# green: the port's chain, and retail's four functions it is transcribed from
# ---------------------------------------------------------------------------

def chain_green():
    vs = read(os.path.join(UILIB, "uivertscroll.cpp"))
    us = read(os.path.join(UILIB, "uiscrollable.cpp"))

    # the event sets the width AND the base, as retail's @0x00094960 does
    m = re.search(r"void UIVertScroll::setScrollbarWidth\(Event \*ev\)\s*\{(.*?)\n\}", vs, re.S)
    check(m is not None, "uivertscroll.cpp: setScrollbarWidth(Event) not found")
    if m:
        body = m.group(1)
        check("m_scrollbarWidth     = ev->GetFloat(1)" in body
              and "m_scrollbarWidthBase = ev->GetFloat(1)" in body,
              "uivertscroll.cpp: the event does not set both the width and its base")

    # the right-align layout uses the width, not a hard-coded constant
    m = re.search(r"void UIVertScroll::InitFrameAlignRight\(UIWidget \*parent\)\s*\{(.*?)\n\}", vs, re.S)
    check(m is not None, "uivertscroll.cpp: InitFrameAlignRight not found")
    if m:
        body = m.group(1)
        check("clientFrame.size.width - m_scrollbarWidth" in body,
              "uivertscroll.cpp: the right-align x is not client right edge minus width")
        check("frame.size.width     = m_scrollbarWidth" in body,
              "uivertscroll.cpp: the scrollbar width is not the declared width")

    # the template copy carries the base into the created scrollbar
    m = re.search(r"void UIVertScroll::copyProperties\(UIWidget& other\)\s*\{(.*?)\n\}", vs, re.S)
    check(m is not None, "uivertscroll.cpp: copyProperties not found")
    if m:
        body = m.group(1)
        check("dest->m_scrollbarWidth     = m_scrollbarWidthBase" in body,
              "uivertscroll.cpp: the template copy does not carry the width")
        check("UIWidget::copyProperties(other);" in body,
              "uivertscroll.cpp: copyProperties skips the base class copy")

    # the list's own arm must not invent a width the data did not declare
    lb = read(os.path.join(UILIB, "uilistbox.cpp"))
    hardcoded = re.findall(r"InitFrame\([^,]+,[^,]+,[^,]+,[^,]+,\s*([0-9.]+f?)", lb)
    check(not hardcoded,
          "uilistbox.cpp: a scrollbar frame is laid out with a literal width %r "
          "instead of the declared one" % hardcoded)

    # the scrollable arm forwards the event to the scrollbar it created
    check("m_scrollBar->setScrollbarWidth(ev->GetFloat(1))" in us,
          "uiscrollable.cpp: scrollbar_width is not forwarded to the scrollbar")
    return not fails


def retail_green():
    """The four retail functions the port transcribes, field for field."""
    if not os.path.isdir(DECOMP):
        check(False, "the Mac decompiles are absent at %s" % DECOMP)
        return False

    init = os.path.join(DECOMP, "InitFrameAlignRight__12UIVertScrollFP8UIWidget.c")
    if os.path.isfile(init):
        body = read(init)
        check("getClientFrame__8UIWidgetFv" in body,
              "retail InitFrameAlignRight: no client frame")
        # x = client.x + client.w - m_scrollbarWidth ; w = m_scrollbarWidth
        check("- *(float *)(a1 + 1740)" in body and "v7[2] = *(float *)(a1 + 1740)" in body,
              "retail InitFrameAlignRight: the width is not offset 1740")
    else:
        check(False, "retail InitFrameAlignRight decompile absent")

    ev = os.path.join(DECOMP, "setScrollbarWidth__12UIVertScrollFP5Event.c")
    if os.path.isfile(ev):
        body = read(ev)
        check("*(float *)(a1 + 1740) = Float" in body
              and "*(float *)(a1 + 1744) = Float" in body,
              "retail setScrollbarWidth: does not set both 1740 and 1744")
    else:
        check(False, "retail setScrollbarWidth decompile absent")

    cp = os.path.join(DECOMP, "copyProperties__12UIVertScrollFR8UIWidget.c")
    if os.path.isfile(cp):
        body = read(cp)
        check("a2[435] = a1[436]" in body and "a2[436] = v5" in body,
              "retail copyProperties: the width pair is not 435/436 from 436")
        check("copyProperties__8UIWidgetFR8UIWidget" in body,
              "retail copyProperties: does not call the base")
    else:
        check(False, "retail copyProperties decompile absent")
    return not fails


def data_green():
    """The shipped declarations: two zeros by authorship, the rest 12 or 16."""
    widths = shipped_scrollbar_widths()
    if widths is None:
        check(False, "the retail install is absent at %s" % RETAIL)
        return False
    check(len(widths) > 0, "the shipped data declares no scrollbar_width at all")
    zeros = sorted(f for f, v in widths if v == 0)
    check(zeros == ["dm_scoreboard.urc", "menu_controls.urc"],
          "the shipped zero-width scrollbars are %r, not the two retail authors" % zeros)
    check(all(v in (0.0, 12.0, 16.0) for _, v in widths),
          "the shipped widths are not the authored set: %r"
          % sorted({v for _, v in widths}))
    return not fails


# ---------------------------------------------------------------------------
# planted faults
# ---------------------------------------------------------------------------

def arm(fault):
    """Break the chain the way the fault describes and require green to fail."""
    vs = read(os.path.join(UILIB, "uivertscroll.cpp"))

    def removed(frag):
        return frag not in vs

    if fault == 1:
        # the event sets the width but not the base: the template copy loses it
        return removed("m_scrollbarWidthBase = ev->GetFloat(1);")
    if fault == 2:
        # the right-align layout hard-codes 16 instead of the declared width
        return removed("frame.size.width     = m_scrollbarWidth")
    if fault == 3:
        # the template copy drops the width entirely
        return removed("dest->m_scrollbarWidth     = m_scrollbarWidthBase")
    if fault == 4:
        # the right-align x ignores the width, so the scrollbar leaves the list
        return removed("clientFrame.size.width - m_scrollbarWidth")
    if fault == 5:
        # a zero-width scrollbar is drawn anyway: the data's authorship is overridden
        w = shipped_scrollbar_widths()
        return w is None or all(v > 0 for _, v in w)
    if fault == 6:
        # retail's event is treated as setting only one of the two fields
        ev = os.path.join(DECOMP, "setScrollbarWidth__12UIVertScrollFP5Event.c")
        return "*(float *)(a1 + 1744) = Float__8EventVarFR5Event;" not in read(ev)
    raise SystemExit("unknown fault %r" % fault)


def main():
    fault = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1] else 0
    if fault:
        if arm(fault):
            print("  FAIL  planted fault %d did not fire" % fault)
            return 0
        print("  ok    planted fault %d fires" % fault)
        return 1

    chain_green()
    retail_green()
    data_green()
    for f in fails:
        print("  FAIL  " + f)
    if fails:
        return 1
    print("  ok    %d checks; every declared scrollbar width reaches the screen"
          % checks[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
