#!/bin/sh
# The loud-refusal control: no fallback in the Vulkan lane switches route
# silently.  The arena's compiled default is off until it accounts for itself,
# its bake refuses a rebase that leaves the surface's own vertex span, and the
# texture-unit disarm that falls back to the white image names itself behind
# the master gate.  The `*default`/`tr.defaultShader` content fallbacks stay --
# retail keeps them (R_FindShader @0x0003DD40 passes the default-on-miss flag
# as the last R_RealFindShader argument) -- so they are deliberately not
# refusals.  Green on the shipping tree, red on every planted fault.  Never
# launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../"

cd "$here"

rc=0

if ! python3 ./refusal_control.py 0 "$src"; then
	echo "  FAIL  the shipping tree is not green"
	rc=1
fi

for m in 1 2 3 4; do
	if ! python3 ./refusal_control.py "$m" "$src" >/dev/null 2>&1; then
		echo "  FAIL  planted fault $m did not go red"
		rc=1
	fi
	git -C "$src" checkout -q -- app/src/main/cpp/code/renderervulkan/vk_vbo.c \
		app/src/main/cpp/code/renderervulkan/vk_shade.c
done

if [ "$rc" -eq 0 ]; then
	echo "  green on the shipping tree, red on 4 planted faults"
fi

exit "$rc"
