#!/bin/sh
# The two transfer call sites the validation layer named: the texel probe's
# block-aligned, correctly-laid-out copy, and the arena bake's copies leaving the
# render pass.  RED when either call site regresses.  Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code/renderervulkan"

cd "$here"

rc=0

if ! python3 ./copysite_control.py 0; then
	echo "  FAIL  the shipping call sites are not green"
	rc=1
fi

for m in 1 2 3 4; do
	if python3 ./copysite_control.py "$m" >/dev/null 2>&1; then
		echo "  FAIL  planted fault $m was not detected"
		rc=1
	else
		echo "  ok    planted fault $m detected"
	fi
done

# source faults: the same breakages in the shipping text
mut=$( mktemp -d )
trap 'rm -rf "$mut"' EXIT

src_fault() {
	# $1 = mutated file, $2 = its slot, $3 = the fault's name
	case "$2" in
	tex)  python3 ./copysite_control.py 0 "$mut/vk_texture.c" >/dev/null 2>&1 ;;
	vbo)  python3 ./copysite_control.py 0 "$src/vk_texture.c" "$mut/vk_vbo.c" >/dev/null 2>&1 ;;
	frame) python3 ./copysite_control.py 0 "$src/vk_texture.c" "$src/vk_vbo.c" "$mut/vk_frame.c" >/dev/null 2>&1 ;;
	esac && {
		echo "  FAIL  source fault $3 was not detected"
		rc=1
	} || echo "  ok    source fault $3 detected"
}

# 5: the bake goes back inside the render pass
sed 's/cmd = VK_StaticXferCmd();/cmd = VK_Shade_CommandBuffer();/' "$src/vk_vbo.c" > "$mut/vk_vbo.c"
src_fault "$mut/vk_vbo.c" vbo "5 the bake back inside the pass"

# 6: the submission stops taking the transfer buffer
sed 's/VK_VboStaticXferClose( slot )/VK_NULL_HANDLE/' "$src/vk_frame.c" > "$mut/vk_frame.c"
src_fault "$mut/vk_frame.c" frame "6 the transfer buffer not submitted"

exit $rc
