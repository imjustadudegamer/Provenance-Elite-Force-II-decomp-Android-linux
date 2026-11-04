# borg-gen-r3: raw PPC disasm of retail's direction-verb path in cgame_mac.i64.
# EF2_FUNCS  = comma list of exact IDA names -> full body disasm
# EF2_DIS    = "start:count;start:count" hex -> raw range disasm (addr, dword, mnemonic)
# EF2_POOL   = "addr;addr;addr" hex -> decode each dword as BIG-ENDIAN float + as int
# EF2_XREF   = comma list of data labels -> dump the qwords they point at
import ida_auto, ida_bytes, ida_funcs, ida_lines, ida_name, ida_xref, idc, os, struct

ida_auto.auto_wait()


def clean(ea):
    s = idc.generate_disasm_line(ea, 0) or ""
    return ida_lines.tag_remove(s)


def dump_fn(nm):
    ea = ida_name.get_name_ea(idc.BADADDR, nm)
    if ea == idc.BADADDR:
        print("=== %s NOT FOUND ===" % nm)
        return
    f = ida_funcs.get_func(ea)
    if not f:
        print("=== %s @0x%X NO FUNC ===" % (nm, ea))
        return
    print("=== %s @0x%X len 0x%X end 0x%X ===" % (nm, f.start_ea, f.size(), f.end_ea))
    a = f.start_ea
    while a < f.end_ea:
        raw = ida_bytes.get_dword(a)
        print("0x%06X: %08X  %s" % (a, raw, clean(a)))
        a = idc.next_head(a, f.end_ea)
        if a == idc.BADADDR:
            break


def dump_range(start, n):
    a = start
    print("/**** RAW 0x%08X len %d ****/" % (start, n))
    while a < start + n:
        raw = ida_bytes.get_dword(a)
        print("  0x%08X  %08X  %s" % (a, raw, clean(a)))
        a = idc.next_head(a, start + n)
        if a == idc.BADADDR:
            break


def dump_pool(items):
    for a in items:
        b = ida_bytes.get_bytes(a, 8) or b""
        fbe = struct.unpack(">f", b[:4])[0]
        fle = struct.unpack("<f", b[:4])[0]
        print("  0x%06X: %s  BE-float=%.6g  LE-float=%.6g  LE-int=%d  raw8=%s"
              % (a, b.hex(), fbe, fle, struct.unpack("<i", b[:4])[0], b.hex()))


def dump_label(nm):
    ea = ida_name.get_name_ea(idc.BADADDR, nm)
    print("--- label %s @0x%X" % (nm, ea))
    if ea == idc.BADADDR:
        return
    for i in range(0, 24, 4):
        v = ida_bytes.get_dword(ea + i)
        s = ""
        try:
            t = ida_bytes.get_strlit_contents(ea + i, 64, 0)
            if t:
                s = repr(t)
        except Exception:
            pass
        print("  +%02d: %08X %s" % (i, v, s))


for nm in [s for s in os.environ.get("EF2_FUNCS", "").split(",") if s]:
    dump_fn(nm)
for item in [s for s in os.environ.get("EF2_DIS", "").split(";") if s]:
    a, n = item.split(":")
    dump_range(int(a, 16), int(n))
for item in [s for s in os.environ.get("EF2_POOL", "").split(";") if s]:
    dump_pool([int(item, 16)])
for nm in [s for s in os.environ.get("EF2_XREF", "").split(",") if s]:
    dump_label(nm)
idc.qexit(0)
