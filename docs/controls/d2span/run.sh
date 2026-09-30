#!/bin/sh
# The 2D batch's state spans: green on the shipping sequencing and model, red on
# each planted fault -- in the handshake the model mirrors, and in the source
# texts themselves.  Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code/renderervulkan"

cd "$here"

rc=0

if ! python3 ./d2span_control.py 0; then
	echo "  FAIL  the shipping sequencing is not green"
	rc=1
fi

# model faults: the handshake the source pins describe, broken in the model
for m in 1 2 3 6; do
	if python3 ./d2span_control.py "$m" >/dev/null 2>&1; then
		echo "  FAIL  model fault $m was not detected"
		rc=1
	else
		echo "  ok    model fault $m detected"
	fi
done

# source faults: mutate the shipping text and require RED
mut=$( mktemp -d )
trap 'rm -rf "$mut"' EXIT

source_fault() {
	# $1 = mutated file, $2 = name; the control is run with the mutation in the
	# slot the fault belongs to (cmds, draw, batch)
	case "$2" in
	4|5|9) python3 ./d2span_control.py 0 "$mut/tr_cmds.c" >/dev/null 2>&1 ;;
	7|8)   python3 ./d2span_control.py 0 "$src/tr_cmds.c" "$mut/tr_backend.c" >/dev/null 2>&1 ;;
	esac && {
		echo "  FAIL  source fault $2 was not detected"
		rc=1
	} || echo "  ok    source fault $2 detected"
}

# 4: the immediate run is no longer closed at a window change
sed 's/RB_2D_FlushImmediate();/RB_2D_Flush();/' "$src/tr_cmds.c" > "$mut/tr_cmds.c"
source_fault "$mut/tr_cmds.c" 4

# 5: the pre-3D flush is gone
sed '/op == RC_DRAW_SURFS || op == RC_SPRITE_SURFS/,/VK_WALK_ADD_NET/ s/RB_2D_Flush();//' \
	"$src/tr_cmds.c" > "$mut/tr_cmds.c"
source_fault "$mut/tr_cmds.c" 5

# 7: the stage pass draws the batch once instead of per span
sed 's/RB_StageDrawSpan( vk.commandBuffers\[s_rbSlot\], indexBase, vertexBase, tess.numIndexes );/VK_Draw_DrawIndexed( vk.commandBuffers[s_rbSlot], tess.numIndexes, indexBase, vertexBase );/' \
	"$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "$mut/tr_backend.c" 7

# 8: the spans outlive the frame they were recorded in
sed '/^void RB_FrameOpen( int slot )/,/^}/ s/RB_2D_SpanReset();//' \
	"$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "$mut/tr_backend.c" 8

# 9: the break call is gone, so the queued quads keep the new state
sed 's/RB_2D_SpanBreak()/RB_2D_SpanBreakX()/' "$src/tr_cmds.c" > "$mut/tr_cmds.c"
source_fault "$mut/tr_cmds.c" 9

# 10 and 11: the draw colour stops riding on the quad.  These are planted in
# tr_shade.c, which the control reads from the shipping tree by default.
colour_fault() {
	python3 ./mutate.py "$1" "$src/tr_shade.c" "$mut/tr_shade.c"
	if python3 ./d2span_control.py 0 "$src/tr_cmds.c" "$src/tr_backend.c" \
		"$src/tr_draw.c" "$mut/tr_shade.c" >/dev/null 2>&1; then
		echo "  FAIL  source fault $2 was not detected"
		rc=1
	else
		echo "  ok    source fault $2 detected"
	fi
}

colour_fault 10 "10 the 2D colour flag dropped"
colour_fault 11 "11 the global-colour 2D branch"

exit $rc
