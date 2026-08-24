#!/usr/bin/env python3
"""Decode an EF2 level-transition persistent file (.spv) with a RETAIL-shaped reader.
Container: RE_SAVEGAME_EF2.md 1/3; payload: SDK g_main.cpp:381-419 (PERSISTANT_VERSION,
gameVars object, then Player::ArchivePersistantData per client).
Object frame = int32 tag(0x0F/0x15) + int32 bodysize + str classname + int32 objIndex + body.
Retail ScriptVariable body = str name + f32 value + str string + vec3 (SDK scriptvariable.h)."""
import struct,sys
d=open(sys.argv[1],'rb').read(); o=0
def i32():
    global o; v=struct.unpack_from('<i',d,o)[0]; o+=4; return v
def f32():
    global o; v=struct.unpack_from('<f',d,o)[0]; o+=4; return v
def vec():
    global o; v=struct.unpack_from('<3f',d,o); o+=12; return v
def s():
    global o; n=i32(); v=d[o:o+n].decode('latin-1'); o+=n; return v
assert i32()==0x00324645, "bad magic"
assert i32()==2, "bad container version"
print("info:", repr(s()))
print("objectCount:", i32())
print("PERSISTANT_VERSION:", i32())
tag=i32(); size=i32(); cls=s(); idx=i32()
print("frame tag=0x%02X size=%d class=%r objIndex=%d"%(tag,size,cls,idx))
body=o
num=i32(); print("  numVariables:", num)
for k in range(num):
    t2=i32(); sz2=i32(); c2=s(); ix2=i32(); b2=o
    nm=s(); val=f32(); st=s(); vc=vec()
    used=o-b2
    print("    %-26s value=%-14g string=%-38r vec=%s  [%d/%d %s]"%(
        nm,val,st,tuple(round(x,6) for x in vc),used,sz2,"OK" if used==sz2 else "MISMATCH"))
print("  gameVars body consumed %d of declared %d  DELTA %d"%(o-body,size,(o-body)-size))
print("bytes left after gameVars (= Player::ArchivePersistantData):", len(d)-o)
