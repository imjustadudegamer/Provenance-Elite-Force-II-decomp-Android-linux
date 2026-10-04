#!/bin/sh
# The world-set census's offline oracle and static invariant.  Green on the
# shipping census, red on every planted fault.  Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code"

cd "$here"

rc=0

build() {
	gcc -std=gnu17 -O1 -g -o "$1" worldsetcensus_oracle.c \
		-I"$src/renderervulkan" -I"$src/qcommon" -I"$src/renderercommon" -I"$src" \
		-I"$src/thirdparty/jpeg-9f" $( pkg-config --cflags sdl2 )
}

# 1. the census's own decision functions, straight out of the shipping source
python3 region_extract.py "$src/renderervulkan/vk_shade.c" 0 > worldset_census_region.inc

if build ./worldsetcensus_oracle; then
	if ./worldsetcensus_oracle; then
		echo "  ok    the shipping census decision, over the shipped stage sets"
	else
		echo "  FAIL  the shipping census decision"
		rc=1
	fi
else
	echo "  FAIL  the oracle does not build"
	rc=1
fi

for m in 1 2 3 4 5; do
	python3 region_extract.py "$src/renderervulkan/vk_shade.c" "$m" > worldset_census_region.inc
	if ! build "./worldsetcensus_oracle_m$m"; then
		echo "  FAIL  planted fault $m does not build"
		rc=1
	elif "./worldsetcensus_oracle_m$m" >/dev/null 2>&1; then
		echo "  FAIL  planted fault $m was not detected"
		rc=1
	else
		echo "  ok    planted fault $m detected"
	fi
done

# the shipping region back in place for the invariant run
python3 region_extract.py "$src/renderervulkan/vk_shade.c" 0 > worldset_census_region.inc

# 2. the static invariant: every iterator path reaches the census, in the gate
if python3 staticinvariant.py "$src"; then
	echo "  ok    the static invariant"
else
	echo "  FAIL  the static invariant"
	rc=1
fi

for m in 1 2 3 4; do
	if python3 staticinvariant.py "$src" --mutate "$m" >/dev/null 2>&1; then
		echo "  FAIL  planted invariant fault $m was not detected"
		rc=1
	else
		echo "  ok    planted invariant fault $m detected"
	fi
done

# 3. the picmip invariant: the shift is the count the allow flag gates
if python3 picmipinvariant.py "$src"; then
	echo "  ok    the picmip invariant"
else
	echo "  FAIL  the picmip invariant"
	rc=1
fi

if python3 picmipinvariant.py "$src" --mutate 1 >/dev/null 2>&1; then
	echo "  FAIL  planted picmip fault was not detected"
	rc=1
else
	echo "  ok    planted picmip fault detected"
fi

rm -f worldset_census_region.inc ./worldsetcensus_oracle ./worldsetcensus_oracle_m1 \
	./worldsetcensus_oracle_m2 ./worldsetcensus_oracle_m3 ./worldsetcensus_oracle_m4 \
	./worldsetcensus_oracle_m5
exit $rc
