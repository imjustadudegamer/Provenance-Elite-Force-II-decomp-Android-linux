# tag-root: find every cgame instruction that touches the spawnthing aim-axis
# offsets recovered from retail's AddTempModels disasm (0x69C / 0x6DC / 0x700),
# and decode the float constants the AddTempModels direction block reads.
import ida_auto, ida_bytes, ida_funcs, ida_lines, ida_ua, ida_segment, idc, os, struct

ida_auto.auto_wait()


def clean(ea):
    s = idc.generate_disasm_line(ea, 0) or ""
    return ida_lines.tag_remove(s)


def dump_pool(items):
    for a in items:
        b = ida_bytes.get_bytes(a, 8) or b""
        fbe = struct.unpack(">f", b[:4])[0]
        fbd = struct.unpack(">d", b)[0]
        print("  0x%06X: %-20s BE-float=%.10g  BE-double=%.10g"
              % (a, b.hex(), fbe, fbd))


WANT = [0x69C, 0x6DC, 0x700, 0x69C + 4, 0x69C + 8]
text = {w: [] for w in WANT}

nfun = ida_funcs.get_func_qty()
print("=== functions: %d ===" % nfun)
for i in range(nfun):
    f = ida_funcs.getn_func(i)
    nm = ida_funcs.get_func_name(f.start_ea)
    a = f.start_ea
    hits = []
    while a < f.end_ea:
        line = clean(a)
        for w in WANT:
            if line.find("0x%X" % w) >= 0 or line.find("0x%x" % w) >= 0:
                hits.append((a, w, line))
        a = idc.next_head(a, f.end_ea)
        if a == idc.BADADDR:
            break
    if hits:
        print("--- %s @0x%X end 0x%X  hits=%d" % (nm, f.start_ea, f.end_ea, len(hits)))
        for (ea, w, line) in hits:
            print("   0x%06X  off=0x%-4X %s" % (ea, w, line))
        text[len(hits)] = text.get(len(hits), [])

print("=== constant pool ===")
dump_pool([0x61608, 0x6160C, 0x61610, 0x61618, 0x6161C, 0x61620, 0x6163C,
           0x6164C, 0x61650, 0x61654,
           0x61670, 0x61678, 0x61680, 0x61688, 0x61C48, 0x61C50, 0x61C68, 0x61C70])

idc.qexit(0)
