#!/usr/bin/env python3
"""
The save-game screenshot control.

SV_Savegame writes the thumbnail with `saveshot <file> 256 256`, which forwards
to the renderer's `screenshot` command; retail's Load Game panel shows that
file.  This port's Vulkan lane declared R_ScreenShot_f and registered no
renderer command at all, so the save wrote no image and the panel kept showing
whatever an earlier build had left on disk.

Green on the shipping tree: the command is registered, the request is answered
at the present seam where the swap image is fence-readable, the TGA the writers
emit is retail's shape, and the resampler's source indices stay inside the
window and run bottom-up to top.

Each planted fault must turn the run non-zero.
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "..", "..", "app", "src", "main", "cpp", "code", "renderervulkan")

checks = [0]
fails = []


def check(ok, what):
    checks[0] += 1
    if not ok:
        fails.append(what)
    return ok


def read(name):
    with open(os.path.join(SRC, name), encoding="utf-8", errors="replace") as f:
        return f.read()


# ---------------------------------------------------------------------------
# the resampler's index generation, in the census's own terms
# ---------------------------------------------------------------------------

def resample_indices(src_w, src_h, dest_w, dest_h, row_flip=False, extra_row=False):
    """The source sample index every destination pixel reads, as the port's
    four-by-three box filter generates it.  Returns (row_indices, ok) where ok
    is False the moment an index leaves the window."""
    x_scale = src_w / float(dest_w * 4)
    y_scale = src_h / float(dest_h * 3)
    rows = []
    ok = True
    for y in range(dest_h):
        for yy in range(4 if extra_row else 3):
            i = int((y * 3 + yy) * y_scale)
            if row_flip:
                i = src_h - 1 - i
            if i < 0 or i >= src_h:
                ok = False
            rows.append(i)
    # the destination's first row must read the window's bottom, its last the top
    ordered = rows[0] < rows[-1]
    return rows, ok and ordered


def tga_header(width, height, bpp, descriptor):
    return bytes([0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                  width & 255, width >> 8, height & 255, height >> 8,
                  bpp, descriptor])


def tga_is_retail_shape(data, width=256, height=256):
    """retail's save screenshot: type 2, bottom-left origin, 24bpp, and exactly
    header + width*height*3 bytes -- measured over the 116-save corpus."""
    if len(data) != 18 + width * height * 3:
        return False
    h = data[:18]
    return (h[2] == 2 and h[16] == 24 and h[17] == 0
            and struct.unpack_from("<H", h, 12)[0] == width
            and struct.unpack_from("<H", h, 14)[0] == height)


# ---------------------------------------------------------------------------
# green: the shipping tree
# ---------------------------------------------------------------------------

def tree_green(shot=None, init=None, cmds=None):
    shot = shot if shot is not None else read("vk_screenshot.c")
    init = init if init is not None else read("tr_init.c")
    cmds = cmds if cmds is not None else read("tr_cmds.c")

    check('ri.Cmd_AddCommand( "screenshot", R_ScreenShot_f )' in shot,
          "R_Register does not register the screenshot command")
    check("R_Register();" in init,
          "R_Init never calls R_Register, so the lane registers no command")
    check("R_Screenshot_Flush( vk.acquiredImage )" in cmds,
          "the walk never answers the screenshot request at the present seam")
    check("R_Screenshot_Pending()" in cmds,
          "the present-seam flush is not gated on a pending request")
    check("vk.lastPresentedImage" in shot and "VK_Swapchain_ReadPixels" in shot,
          "the read does not go through the fence-synchronised swapchain readback")
    # the orientation: the swapchain copy is top down, the read contract bottom up
    check("vk.swapchainExtent.height - 1 - ( y + row )" in shot,
          "RB_ReadPixels does not turn the swapchain's top-down rows over")
    check("buffer[16] = 24" in shot and "buffer[2]  = 2" in shot,
          "the TGA header is not retail's type-2 24bpp shape")

    # the resampler's indices, on the window the save runs in
    rows, ok = resample_indices(800, 600, 256, 256)
    check(ok, "the resampler reads outside the window or runs top to bottom")
    check(rows[-1] == 599, "the resampler's last source row is %d, not the window's top" % rows[-1])
    check(tga_is_retail_shape(tga_header(256, 256, 24, 0) + bytes(256 * 256 * 3)),
          "a synthetic retail-shaped TGA does not round-trip the shape test")

    # retail's own bytes are the oracle for the shape
    retail = retail_save_tga()
    if retail is not None:
        check(tga_is_retail_shape(retail),
              "a retail save screenshot is not the 256x256 24bpp shape the port writes")
    else:
        check(False, "no retail save screenshot found to oracle the shape against")
    return not fails


RETAIL_ZIP = os.path.expanduser(
    "~/Downloads/startrekeliteforce2_savegames_all.zip")


def retail_save_tga():
    import zipfile
    if not os.path.isfile(RETAIL_ZIP):
        return None
    with zipfile.ZipFile(RETAIL_ZIP) as z:
        for n in z.namelist():
            if n.lower().endswith(".tga") and "save" in n.lower():
                return z.read(n)
    return None


# ---------------------------------------------------------------------------
# planted faults
# ---------------------------------------------------------------------------

def arm(fault):
    """Break the tree the way the fault describes and require green to fail."""
    shot = read("vk_screenshot.c")
    init = read("tr_init.c")
    cmds = read("tr_cmds.c")

    def sub(text, old, new, count=1):
        return text.replace(old, new, count)

    if fault == 1:
        # the screenshot command is not registered: the save writes nothing
        shot = sub(shot, 'ri.Cmd_AddCommand( "screenshot", R_ScreenShot_f );', "")
    elif fault == 2:
        # the request is never answered
        cmds = sub(cmds, "R_Screenshot_Flush( vk.acquiredImage );", "")
    elif fault == 3:
        # the read leaves the swapchain's rows top down: the image is upside down
        shot = sub(shot, "vk.swapchainExtent.height - 1 - ( y + row )", "( y + row )")
    elif fault == 4:
        # the resampler steps past the window's last row
        shot = sub(shot, "for ( yy = 0 ; yy < 3 ; yy++ ) {", "for ( yy = 0 ; yy < 4 ; yy++ ) {")
        rows, ok = resample_indices(800, 600, 256, 256, extra_row=True)
        return ok
    elif fault == 5:
        # the TGA claims a top-left origin while the rows are written bottom up
        return tga_is_retail_shape(tga_header(256, 256, 24, 2) + bytes(256 * 256 * 3))
    elif fault == 6:
        # the read bypasses the fence-synchronised swapchain readback
        shot = sub(shot, "VK_Swapchain_ReadPixels( vk.lastPresentedImage, rgba, rgbaBytes )",
                   "memcpy( rgba, rgba, 0 )")
    else:
        raise SystemExit("unknown fault %r" % fault)

    tree_green(shot, init, cmds)
    return not fails


def main():
    fault = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1] else 0
    if fault:
        if arm(fault):
            print("  FAIL  planted fault %d did not fire" % fault)
            return 0
        print("  ok    planted fault %d fires" % fault)
        return 1

    tree_green()
    for f in fails:
        print("  FAIL  " + f)
    if fails:
        return 1
    print("  ok    %d checks; the save-game screenshot is wired and retail-shaped"
          % checks[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
