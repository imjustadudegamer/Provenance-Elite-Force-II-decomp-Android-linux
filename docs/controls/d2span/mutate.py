#!/usr/bin/env python3
"""Plant a source fault in a copy of tr_shade.c for the d2span control.

The colour faults are multi-line, so they are done here rather than in sed:

    mutate.py 10 <src> <out>   the 2D resolution stops flagging the quad colour
    mutate.py 11 <src> <out>   one global generator stops honouring the flag
"""
import sys

fault = int(sys.argv[1])
src = open(sys.argv[2], encoding="utf-8", errors="replace").read()

if fault == 10:
    old = "\ts_2DColorFromVertex = qtrue;\n"
    assert old in src, "the 2D colour flag is not set"
    src = src.replace(old, "", 1)
elif fault == 11:
    old = "if ( s_2DColorFromVertex ) {"
    assert src.count(old) == 2, "the two global generators"
    src = src.replace(old, "if ( 0 ) {", 1)
else:
    raise SystemExit("mutate.py: no fault %d" % fault)

open(sys.argv[3], "w", encoding="utf-8").write(src)
