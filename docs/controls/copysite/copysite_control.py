#!/usr/bin/env python3
"""
copysite_control.py -- the two transfer call sites the validation layer named.

Two VUID classes were left in the armed run:

  * `vkCmdCopyImageToBuffer-srcImageLayout-01397`, `-srcImage-00207/00208`,
    `-imageOffset-07747` -- the texel probe copied a 1x1 region at (x,y) from a
    block-compressed 256x256 image, declaring the source as
    SHADER_READ_ONLY_OPTIMAL.  A copy source must be in a transfer layout, and a
    compressed image's texel lives inside a block: the region has to be the block
    that holds it, aligned down, with an extent that is a whole number of blocks
    or reaches the subresource edge.
  * `vkCmdCopyBuffer-renderpass` -- the static arena's bake copied the ring's
    bytes into the device buffer from inside the frame's render pass.

This control pins both call sites and checks the alignment arithmetic the first
one needs: for every block size in use, the offset is a multiple of the block
and the extent is a whole number of blocks or ends exactly at the image edge --
the two clauses 00207/00208/07747 state.

Planted faults break each in turn (argv[1]), and the same in the source text
(argv[2:] = texture, vbo, frame).

Build/run: docs/controls/copysite/run.sh  (never launches the game)
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
WT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
VK = os.path.join(WT, "app", "src", "main", "cpp", "code", "renderervulkan")

TEXTURE = os.path.join(VK, "vk_texture.c")
VBO = os.path.join(VK, "vk_vbo.c")
FRAME = os.path.join(VK, "vk_frame.c")

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


def body(text, header):
    i = text.find(header)
    if i < 0:
        return ""
    j = text.find("\n}\n", i)
    return text[i:j + 3] if j > 0 else text[i:]


def main():
    mutant = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    tex_path = sys.argv[2] if len(sys.argv) > 2 else TEXTURE
    vbo_path = sys.argv[3] if len(sys.argv) > 3 else VBO
    frame_path = sys.argv[4] if len(sys.argv) > 4 else FRAME

    tex = read(tex_path)
    vbo = read(vbo_path)
    frame = read(frame_path)

    # --- the texel probe ---------------------------------------------------
    fn = body(tex, "qboolean VK_Texture_ReadTexel(")
    check(bool(fn), "VK_Texture_ReadTexel is gone")

    if mutant == 1:
        fn = fn.replace("bx = ( x / bw ) * bw;", "bx = x;")
    if mutant == 2:
        fn = fn.replace("VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL",
                        "VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL", 1)

    check("bx = ( x / bw ) * bw;" in fn and "by = ( y / bh ) * bh;" in fn,
          "the read texel's region is not block-aligned, so a block-compressed "
          "image violates its own block extent (VUID-srcImage-00207/00208)")
    check("VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL" in fn,
          "the copy does not declare a transfer source layout "
          "(VUID-srcImageLayout-01397)")
    check("barrier.oldLayout = tex->layout;" in fn,
          "the barrier does not name the layout the image is actually in")
    check(fn.count("VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL") >= 3,
          "the layout is not transitioned in, used by the copy, and restored")
    check("extentW" in fn and "extentH" in fn,
          "the copied extent is not clipped to the subresource edge")

    # --- the arithmetic the three VUIDs state ------------------------------
    #
    # offset must be a multiple of the block extent, and the extent must be a
    # whole number of blocks or end exactly at the subresource edge.
    blocks = {4: 4, 8: 4, 16: 4}          # BC1 is 8 B, BC2/BC3 16 B: 4x4 blocks
    for bytesPerBlock, block in sorted(blocks.items()):
        for (w, h) in ((256, 256), (128, 64), (16, 16), (2, 2), (1, 1), (4096, 2048)):
            for (x, y) in ((128, 128), (0, 0), (1, 1), (3, 3), (255, 255), (w - 1, h - 1), (w // 2, h // 2)):
                if x >= w or y >= h:
                    continue
                bw = bh = block
                bx = (x // bw) * bw
                by = (y // bh) * bh
                extentW = min(bw, w - bx)
                extentH = min(bh, h - by)
                check(bx % bw == 0 and by % bh == 0,
                      "block offset (%d,%d) is not block aligned for %dx%d" % (bx, by, w, h))
                okW = (extentW % bw == 0) or (bx + extentW == w)
                okH = (extentH % bh == 0) or (by + extentH == h)
                check(okW and okH,
                      "extent %dx%d at (%d,%d) in %dx%d is neither a whole number of "
                      "blocks nor at the edge" % (extentW, extentH, bx, by, w, h))
                check(bx + extentW <= w and by + extentH <= h,
                      "the region leaves the subresource")

    # --- the bake's copies -------------------------------------------------
    bake = body(vbo, "static qboolean VK_StaticBake(")
    if mutant == 3:
        bake = bake.replace("VK_StaticXferCmd();", "VK_Shade_CommandBuffer();")
    check("VK_StaticXferCmd();" in bake and "VK_Shade_CommandBuffer();" not in bake,
          "the bake records its buffer copies into the frame buffer, which is "
          "inside a render pass (VUID-vkCmdCopyBuffer-renderpass)")
    submit = body(frame, "qboolean VK_Frame_Submit( int slot )")
    if mutant == 4:
        submit = submit.replace("buffers[count++] = xfer;", "")
        submit = submit.replace("VK_VboStaticXferClose( slot )", "VK_NULL_HANDLE")
    check("VK_VboStaticXferClose( slot )" in submit,
          "the frame submission does not take the bake's transfer buffer")
    check("buffers[count++] = xfer;" in submit and
          submit.find("buffers[count++] = xfer;") < submit.find("buffers[count++] = vk.commandBuffers[slot];"),
          "the transfer buffer is not submitted ahead of the frame buffer, so the "
          "copies do not precede the draws that read the arena")
    print("copysite control: %d checks, %d failures" % (checks, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
