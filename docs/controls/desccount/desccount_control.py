#!/usr/bin/env python3
"""
desccount_control.py -- the dynamic-offset contract, proven offline.

Vulkan 1.3, `vkspec.txt` line 95811:

    VUID-vkCmdBindDescriptorSets-dynamicOffsetCount-00359
    dynamicOffsetCount must be equal to the total number of dynamic descriptors
    in pDescriptorSets

and lines 95756-95766: the effective offset is "the sum of the relative offset
taken from pDynamicOffsets, and the base address of the buffer plus base offset
in the descriptor set".  So a layout that declares a dynamic uniform buffer must
be bound with exactly one offset (that is how the ring slice reaches the stage
block), and a layout that declares none must be bound with none.

This renderer has two descriptor layouts with exactly that difference -- the
shade layout's binding 0 is VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, the
2D/immediate layout is a single COMBINED_IMAGE_SAMPLER -- and the bind used to
pass 1 for both.  The operator's validation run caught it 105,618 times, and an
invalid bind is not the bind the caller asked for.

This control reads both layouts and both call sites out of the shipping source
and checks the count each call passes against the count its layout declares.
Build/run: docs/controls/desccount/run.sh
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
WT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
RV = os.path.join(WT, "app", "src", "main", "cpp", "code", "renderervulkan")

failures = 0
checks = 0


def check(cond, msg):
    global failures, checks
    checks += 1
    if not cond:
        failures += 1
        print("  FAIL  " + msg)


def read(name):
    with open(os.path.join(RV, name), encoding="utf-8", errors="replace") as f:
        return f.read()


def dynamic_count(text, layout_var, layout_create_call):
    """How many dynamic descriptors a layout declares, read from its creation.

    Both layouts here declare one descriptor per binding, so the number of
    dynamic descriptors is the number of bindings whose descriptorType is a
    *_DYNAMIC type in the run-up to this create call.  The region is bounded to
    that call so a binding array in some other function cannot answer for this
    layout."""
    i = text.find(layout_create_call)
    if i < 0:
        return None
    region = text[max(0, i - 2500):i]
    if "descriptorType" not in region:
        return None
    return len(re.findall(r"descriptorType\s*=\s*VK_DESCRIPTOR_TYPE_\w*_DYNAMIC", region))


def call_count(text, callee):
    """The dynamicCount argument of every VK_Draw_BindSet call, with its layout."""
    out = []
    for m in re.finditer(r"VK_Draw_BindSet\(\s*cmd\s*,\s*([\w\.]+)\s*,\s*([\w\.]+)\s*,\s*([^,]+),\s*([^)]+)\)", text):
        out.append((m.group(1), m.group(4).strip()))
    return out


def main():
    mutant = int(sys.argv[1]) if len(sys.argv) > 1 else 0

    draw = read("vk_draw.c")
    shade = read("vk_shade.c")

    # --- what each layout declares -----------------------------------------
    draw_dyn = dynamic_count(draw, "vk_draw.setLayout", "CreateDescriptorSetLayout")
    shade_dyn = dynamic_count(shade, "vk_shadeSetLayout", "CreateDescriptorSetLayout")

    if mutant == 3:
        shade_dyn = 0            # planted: the shade layout loses its DYNAMIC type

    check(draw_dyn == 0,
          "the 2D/immediate layout declares %s dynamic descriptors, want 0" % draw_dyn)
    check(shade_dyn == 1,
          "the shade layout declares %s dynamic descriptors, want 1" % shade_dyn)

    # --- what each call passes ---------------------------------------------
    calls = call_count(draw, "VK_Draw_BindSet") + call_count(shade, "VK_Draw_BindSet")
    check(len(calls) == 2, "expected the two VK_Draw_BindSet call sites, found %d" % len(calls))

    for layout, count in calls:
        want = draw_dyn if "draw" in layout else shade_dyn
        if mutant == 1 and "draw" in layout:
            count = "1"          # planted: the 2D bind passes an offset again
        if mutant == 2 and "shade" in layout:
            count = "0"          # planted: the ring offset is dropped
        check(count == str(want),
              "VK_Draw_BindSet(%s, ..., %s) passes dynamicCount=%s but the layout declares %d"
              % (layout, count, count, want))

    # --- the bind must forward the count it was given ----------------------
    body = draw[draw.find("void VK_Draw_BindSet("):]
    body = body[:body.find("\n}\n")]
    check("dynamicCount, &dynamicOffset" in body,
          "the bind does not forward its own count to vkCmdBindDescriptorSets")
    check("dynamicCount > 1" in body or "dynamicCount <= 1" in body,
          "the bind does not bound the count to its single binding")
    check("s_boundSetDynCount" in body,
          "the elision cache does not key on the count, so a differing bind could be skipped")

    print("desccount control: %d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
