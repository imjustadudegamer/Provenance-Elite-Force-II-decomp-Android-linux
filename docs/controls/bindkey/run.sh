#!/bin/sh
# The per-bind pipeline lookup: the probe comes first and a hit returns before
# the program lookup.  RED when the lookup moves back, when the key stops
# carrying the lookup's input, or when the permutation census loses a path.
# Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code/renderervulkan"

cd "$here"

rc=0

if ! python3 ./bindkey_control.py 0; then
	echo "  FAIL  the shipping ordering is not green"
	rc=1
fi

for m in 1 2 3 4; do
	if python3 ./bindkey_control.py "$m" >/dev/null 2>&1; then
		echo "  FAIL  planted fault $m was not detected"
		rc=1
	else
		echo "  ok    planted fault $m detected"
	fi
done

# source faults: the same breakages in the shipping text
mut=$( mktemp -d )
trap 'rm -rf "$mut"' EXIT

expect_red() {
	# $1 = the mutated file, $2 = the fault's name
	if python3 ./bindkey_control.py 0 "$mut/vk_shade.c" >/dev/null 2>&1; then
		echo "  FAIL  source fault $2 was not detected"
		rc=1
	else
		echo "  ok    source fault $2 detected"
	fi
}

# 5: the program lookup moves back before the probe
python3 ./mutate.py 5 "$src/vk_shade.c" "$mut/vk_shade.c"
expect_red "$mut/vk_shade.c" "5 the lookup back before the probe"

# 6: the key loses the family
sed 's/pkey\[1\] = (uintptr_t)family;/pkey[1] = 0;/' "$src/vk_shade.c" > "$mut/vk_shade.c"
expect_red "$mut/vk_shade.c" "6 the key lost the family"

# 7: the hit path loses its census note
python3 ./mutate.py 7 "$src/vk_shade.c" "$mut/vk_shade.c"
expect_red "$mut/vk_shade.c" "7 the hit path lost the census note"

exit $rc
