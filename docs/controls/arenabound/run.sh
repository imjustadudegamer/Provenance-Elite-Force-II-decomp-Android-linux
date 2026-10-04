#!/bin/sh
# The registration boundary: the arena purge must be parked, and every table
# that indexes the freed arena must be reset or rebuilt.  The command buffers
# must not come from the arena at all.  RED when a lifetime is missing, when a
# table keeps a pointer into the freed arena, or when the arena allocation
# ledger grows a site outside the destroyed-and-rebuilt set.  Never launches the
# game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code"

cd "$here"

rc=0

if ! python3 ./arenabound_control.py 0; then
	echo "  FAIL  the shipping sources are not green"
	rc=1
fi

for m in 1 2 3 4 5 6 7 8; do
	if python3 ./arenabound_control.py "$m" >/dev/null 2>&1; then
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
	if python3 ./arenabound_control.py 0 "$@" >/dev/null 2>&1; then
		echo "  FAIL  source fault $name was not detected"
		rc=1
	else
		echo "  ok    source fault $name detected"
	fi
}

# 9: the command buffers go back into the arena the boundary frees
sed 's/ri\.Malloc( sizeof( vkCommandList_t ) )/ri.Hunk_Alloc( sizeof( vkCommandList_t ), h_low )/' \
	"$src/renderervulkan/tr_cmds.c" > "$mut/tr_cmds.c"
src_fault "9 the command buffers are arena-allocated again" "$mut/tr_cmds.c"

# 10: the boundary stops parking the walk thread
sed 's/^[[:space:]]*VKimp_FrontEndWaitIdle();$//' \
	"$src/renderervulkan/tr_init.c" > "$mut/tr_init.c"
src_fault "10 the boundary stops parking the walk thread" "$mut/tr_init.c"

exit $rc
