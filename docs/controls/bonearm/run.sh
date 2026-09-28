#!/bin/sh
# The skeleton arena's per-frame property: the shipping tiki/tiki_mesh.cpp is
# linked in (real arena, real cursor, real reset), the reset's call site is read
# out of both shipping lanes, and a two-view frame is driven through the arena.
# RED on the per-view rewind, GREEN when the reset is where a frame boundary is.
# Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code"

cd "$here"

g++ -std=gnu++17 -O1 -g -o ./bonearm_oracle bonearm_oracle.c \
	"$src/tiki/tiki_mesh.cpp" \
	-I"$src" -I"$src/tiki" -I"$src/corepp" -I"$src/skeletor" -I"$src/qcommon" -I"$src/renderercommon"

./bonearm_oracle 0 "$src"
rc=0

for m in 1 2 3; do
	if ./bonearm_oracle "$m" "$src" >/dev/null 2>&1; then
		echo "  FAIL  planted fault $m was not detected"
		rc=1
	else
		echo "  ok    planted fault $m detected"
	fi
done

rm -f ./bonearm_oracle
exit $rc
