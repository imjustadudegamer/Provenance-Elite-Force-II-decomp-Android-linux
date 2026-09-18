#!/usr/bin/env python3
"""A recorded draw must carry its own push constants.

The immediate stream is the one place a draw can be recorded without the ortho
its shader reads: RB_ImmBeginWithImage binds the pipeline and RB_ImmCommit draws.
If the push is nested inside a conditional, some run skips it and inherits
whatever the previous run left -- which is how a 2D quad came to draw under a
stale ortho and land as a garbage sheet across the frame.

The invariant is structural and checkable without a boot: the CmdPushConstants
call sits at function body scope, at the same brace depth as the pipeline bind it
precedes.  A push nested even one brace deeper is conditional on something, and
whatever that something is, a run can reach the bind without taking it.

Exit 0 holds, 1 broken.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "app", "src", "main", "cpp", "code", "renderervulkan",
				   "tr_backend.c")

FUNCTION = "RB_ImmBeginWithImage"
BIND = "CmdBindPipeline"
PUSH = "CmdPushConstants"
# the value the run has to carry inside 2D: the active window's own ortho, which
# the 3D getters do not hold.  Without this reference the run pushes proj x view
# in 2D and every ROQ frame and UI quad lands off the screen.
TWO_D_VALUE = "VK_Draw_CurrentOrtho"


def function_span(lines, name):
	"""Body of `name`, as (open_brace_line, close_brace_line), 0-based."""
	sig = None
	for i, l in enumerate(lines):
		if re.match(r'^\w[\w \t\*]*\b' + name + r'\s*\(', l):
			sig = i
			break
	if sig is None:
		return None
	depth = 0
	opened = False
	for j in range(sig, len(lines)):
		depth += lines[j].count('{') - lines[j].count('}')
		if depth == 1 and not opened:
			opened = True
			start = j
		if opened and depth == 0:
			return start, j
	return None


def calls(lines, start, end, name):
	"""First line (1-based) of each call, with its brace depth."""
	out = {}
	depth = 0
	for j in range(start, end + 1):
		code = lines[j].split('//')[0]
		for ch in code:
			if ch == '{':
				depth += 1
			elif ch == '}':
				depth -= 1
				continue
		for call in name:
			if call not in out and re.search(r'\b' + call + r'\s*\(', code):
				out[call] = (j + 1, depth)
	return out


def main():
	lines = open(SRC, encoding='utf-8', errors='replace').read().splitlines()
	span = function_span(lines, FUNCTION)
	if span is None:
		print("push-invariant: %s not found" % FUNCTION)
		return 1
	start, end = span

	found = calls(lines, start, end, (PUSH, BIND))
	print("push-invariant: %s body lines %d-%d" % (FUNCTION, start + 1, end + 1))
	if BIND not in found:
		print("  FAIL: no %s in %s" % (BIND, FUNCTION))
		return 1
	if PUSH not in found:
		print("  FAIL: no %s -- the draw would run on a stale push" % PUSH)
		return 1

	pline, pdepth = found[PUSH]
	bline, bdepth = found[BIND]
	print("  %-22s line %-5d brace depth %d" % (PUSH, pline, pdepth))
	print("  %-22s line %-5d brace depth %d" % (BIND, bline, bdepth))

	if pdepth != bdepth:
		print("  FAIL: the push is nested inside the bind's scope, so a run can "
			  "reach the bind without taking it and draw on a stale ortho")
		return 1
	if pline > bline:
		print("  FAIL: the push follows the bind, so the first run draws before "
			  "its ortho is recorded")
		return 1
	if not re.search(r'\b' + TWO_D_VALUE + r'\s*\(', "\n".join(lines[start:end + 1])):
		print("  FAIL: the push never reads %s, so an in-2D run carries the 3D "
			  "product and the ROQ and UI quads land off the screen" % TWO_D_VALUE)
		return 1
	print("  ok: the push is at function scope, precedes the bind, and carries "
		  "the window's own ortho inside 2D")
	return 0


if __name__ == "__main__":
	sys.exit(main())
