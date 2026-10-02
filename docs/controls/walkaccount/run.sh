#!/bin/sh
# The walk split closes inside one drain: the nested-subtraction rule must
# survive the front end draining the counters while a window is open.  RED when
# the rule, its users, or the model regress.  Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code/renderervulkan"

cd "$here"

rc=0

if ! python3 ./walkaccount_control.py 0; then
	echo "  FAIL  the shipping accounting is not green"
	rc=1
fi

for m in 1 2 3; do
	if python3 ./walkaccount_control.py "$m" >/dev/null 2>&1; then
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
	# $1 = the fault's name; the mutated copies are the arguments after
	name=$1
	shift
	python3 ./walkaccount_control.py 0 "$@" >/dev/null 2>&1 && {
		echo "  FAIL  source fault $name was not detected"
		rc=1
	} || echo "  ok    source fault $name detected"
}

# 4: VK_WALK_ADD_NET subtracts the raw cumulative counter again
sed 's/VK_WalkStat_NestedSince( n0 );/VK_WalkStat_Nested() - (n0);/' \
	"$src/vk_local.h" > "$mut/vk_local.h"
name="4 the walk wrapper subtracts the raw counter"
src_fault "$name" "$mut/vk_local.h"

# 5: the shade windows subtract the raw cumulative counter again
sed 's/VK_WalkStat_NestedSince( nested0 )/( VK_WalkStat_Nested() - nested0 )/' \
	"$src/tr_backend.c" > "$mut/tr_backend.c"
name="5 the shade window subtracts the raw counter"
src_fault "$name" "$src/vk_local.h" "$src/tr_cmds.c" "$mut/tr_backend.c"

# 6: the rule stops handling a drain inside the window
sed 's/return ( n < n0 ) ? n : ( n - n0 );/return n - n0;/' \
	"$src/tr_cmds.c" > "$mut/tr_cmds.c"
name="6 the rule no longer handles a mid-window drain"
src_fault "$name" "$src/vk_local.h" "$mut/tr_cmds.c" "$src/tr_backend.c"

exit $rc
