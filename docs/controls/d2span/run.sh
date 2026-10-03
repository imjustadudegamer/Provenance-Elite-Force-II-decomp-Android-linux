#!/bin/sh
# The 2D batch's state spans: green on the shipping sequencing and model, red on
# each planted fault -- in the handshake the model mirrors, in the source texts
# themselves, and in the one placement site every 2D writer (the surface batch,
# the immediate stream and the font/glyph runs) ends with.  Never launches.
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
for m in 1 2 3 6 12 13 15 16 19 20 21; do
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
	4|5|9|21) python3 ./d2span_control.py 0 "$mut/tr_cmds.c" "$src/tr_backend.c" "$mut/tr_draw.c" >/dev/null 2>&1 ;;
	7|8|19|20) python3 ./d2span_control.py 0 "$src/tr_cmds.c" "$mut/tr_backend.c" >/dev/null 2>&1 ;;
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

# 7: the stage pass draws the batch once instead of per span.  The call is two
# lines (the coverage census made the parameter list long), so the mutation
# brackets the continuation rather than matching one line.
sed -e 's#RB_StageDrawSpan( vk.commandBuffers\[s_rbSlot\], indexBase, vertexBase, tess.numIndexes,#VK_Draw_DrawIndexed( vk.commandBuffers[s_rbSlot], tess.numIndexes, indexBase, vertexBase );/*#' \
    -e 's#^[[:space:]]*pipeline, stage, image );#						  */#' \
	"$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "$mut/tr_backend.c" 7

# 8: the spans outlive the frame they were recorded in
sed '/^void RB_FrameOpen( int slot )/,/^}/ s/RB_2D_SpanReset();//' \
	"$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "$mut/tr_backend.c" 8

# 21: the purge flushes again, so the arena-free notify draws
sed '/^void RB_2D_OwnersPurged( void ) {/,/^}/ s/RB_2D_SpanReset();/RB_2D_Flush();/' \
	"$src/tr_draw.c" > "$mut/tr_draw.c"
source_fault "$mut/tr_draw.c" 21

# 19: the span's state is not re-applied before its draw, so a batch draws
# under whatever state the command buffer was left in
sed 's/VK_Draw_2DStateApply( cmd, ortho, win, scn );//' \
	"$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "$mut/tr_backend.c" 19

# 20: the census echoes the span's own scissor instead of reading the rect
# Vulkan was given, so it can never report a foreign clip
sed 's/VK_Draw_LastScissorRect( clip );/memcpy( clip, scn, sizeof( clip ) );/' \
	"$src/tr_backend.c" > "$mut/tr_backend.c"
source_fault "$mut/tr_backend.c" 20

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

# 17: the replay guard is gone, so a restored span becomes the state the next
# quad's snapshot reads back
sed 's/vk_2dReplaying = qtrue;//; s/vk_2dReplaying = qfalse;//' \
	"$src/vk_draw.c" > "$mut/vk_draw.c"
if python3 ./d2span_control.py 0 "$src/tr_cmds.c" "$src/tr_backend.c" "$src/tr_draw.c" \
	"$src/tr_shade.c" "$src/tr_font.cpp" "$src/tr_util.cpp" "" "$mut/vk_draw.c" >/dev/null 2>&1; then
	echo "  FAIL  source fault 17 was not detected"
	rc=1
else
	echo "  ok    source fault 17 detected"
fi

# 18: the quad no longer records its own state, so the span is guessed later
sed 's/RB_2D_SpanQuad();//' "$src/tr_draw.c" > "$mut/tr_draw.c"
if python3 ./d2span_control.py 0 "$src/tr_cmds.c" "$src/tr_backend.c" "$mut/tr_draw.c" \
	"$src/tr_shade.c" "$src/tr_font.cpp" "$src/tr_util.cpp" >/dev/null 2>&1; then
	echo "  FAIL  source fault 18 was not detected"
	rc=1
else
	echo "  ok    source fault 18 detected"
fi

# 14: the glyph run's placement is gone, so a text run's quads stay in
# widget-local coordinates while the widget's scissor is its screen rect
sed 's/RB_2DPlaceRange( tess.numVertexes, 4 );//' "$src/tr_font.cpp" > "$mut/tr_font.cpp"
if python3 ./d2span_control.py 0 "$src/tr_cmds.c" "$src/tr_backend.c" "$src/tr_draw.c" \
	"$src/tr_shade.c" "$mut/tr_font.cpp" >/dev/null 2>&1; then
	echo "  FAIL  source fault 14 was not detected"
	rc=1
else
	echo "  ok    source fault 14 detected"
fi

exit $rc
