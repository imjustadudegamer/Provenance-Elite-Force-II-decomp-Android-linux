#!/bin/sh
# Cross-mode render state: the map-change boundary must destroy what the
# reference twin destroys there, and the gated census must name what survives.
# RED when a registration-time invalidation is missing, when the census is
# ungated/unemitted, or when it stops naming the world/view/image state.
# Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code"

cd "$here"

rc=0

if ! python3 ./mapstate_control.py 0; then
	echo "  FAIL  the shipping sources are not green"
	rc=1
fi

for m in 1 2 3 4 5 6 7 8; do
	if python3 ./mapstate_control.py "$m" >/dev/null 2>&1; then
		echo "  ok    planted fault $m detected"
	else
		echo "  FAIL  planted fault $m was not detected"
		rc=1
	fi
done

# source faults: the same breakages in the shipping text
mut=$( mktemp -d )
trap 'rm -rf "$mut"' EXIT

src_fault() {
	name=$1
	shift
	python3 ./mapstate_control.py 0 "$@" >/dev/null 2>&1 && {
		echo "  FAIL  source fault $name was not detected"
		rc=1
	} || echo "  ok    source fault $name detected"
}

# 9: the twin loses the model free too, so neither lane does it
sed 's/^\tR_FreeNonTikiModels();$/\t\/\* R_FreeNonTikiModels(); \*\//' \
	"$src/renderervulkan/tr_init.c" > "$mut/tr_init.c"
name="9 neither lane frees the non-tiki models"
src_fault "$name" "$mut/tr_init.c"

# 10: the census loses a named handle
mkdir -p "$mut/f10"
sed 's/worldMapLoaded=%d //' "$src/renderervulkan/tr_init.c" > "$mut/f10/tr_init.c"
name="10 the census stops naming worldMapLoaded"
src_fault "$name" "$mut/f10/tr_init.c"

exit $rc
