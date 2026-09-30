#!/usr/bin/env python3
"""Plant a source fault in a copy of vk_shade.c for the bindkey control.

The two faults that are not a single-token substitution are done here rather
than in sed: one moves a block, the other removes a line inside a block.

    mutate.py 5 <src> <out>   the program lookup moves back before the probe
    mutate.py 7 <src> <out>   the hit path loses its census note
"""
import sys

fault = int(sys.argv[1])
src = open(sys.argv[2], encoding="utf-8", errors="replace").read()

if fault == 5:
    lookup = """	if ( !VK_Program_Modules( family, defines, &vert, &frag ) ) {
		/* the mask names the permutation the offline build has to add, and the
		   shader names where the content asked for it: a next gap is one boot
		   from diagnosed instead of one census from inferred */
		VK_Shade_NotePerm( 1 );
		VK_Shade_PermRow( shader, family, defines );
		return VK_NULL_HANDLE;
	}
	VK_Shade_NotePerm( 0 );
"""
    probe = "\t/* The probe comes first,"
    assert lookup in src and probe in src, "fault 5: the block moved"
    src = src.replace(lookup, "", 1)
    src = src.replace(probe, lookup + probe, 1)
elif fault == 7:
    old = "\t\t\tVK_Shade_NotePerm( 0 );\n\t\t\treturn vk_shadePipelines[at].pipeline;"
    assert old in src, "fault 7: the hit path moved"
    src = src.replace(old, "\t\t\treturn vk_shadePipelines[at].pipeline;", 1)
else:
    raise SystemExit("mutate.py: no fault %d" % fault)

open(sys.argv[3], "w", encoding="utf-8").write(src)
