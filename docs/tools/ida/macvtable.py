import ida_auto, idc, ida_bytes, ida_name, os
ida_auto.auto_wait()
slot = int(os.environ.get("EF2_VTSLOT", "0"), 16)
vt = ida_bytes.get_dword(slot)
print("/**** vtable via TOC slot 0x%08X -> 0x%08X ****/" % (slot, vt))
for i in range(58):
    ea = vt + 4 * i
    v = ida_bytes.get_dword(ea)
    nm = ida_name.get_name(v) or ""
    extra = ""
    if v and not (0x40000 < v < 0x200000):  # not code: TVector object
        words = [ida_bytes.get_dword(v + 4 * k) for k in range(4)]
        names = [ida_name.get_name(w) or ("0x%X" % w) for w in words]
        extra = "  TVec: " + " | ".join(names)
    print("  %3d  slot +0x%02X  word 0x%08X  %s%s" % (i, 4 * i, v, nm, extra))
idc.qexit(0)
