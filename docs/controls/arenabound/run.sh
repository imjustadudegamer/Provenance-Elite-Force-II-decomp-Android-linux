#!/bin/sh
# The registration boundary: the arena purge must be parked, and every table
# that indexes the freed arena must be reset or rebuilt.  The command buffers
# must not come from the arena at all.  RED when a lifetime is missing, when a
# table keeps a pointer into the freed arena, or when the arena allocation
# ledger grows a site outside the destroyed-and-rebuilt set.
#
# Two oracles over the same class:
#   * a source-invariant checker over the shipping tree (the ledger, the park,
#     the tables, the free sites, the client revalidation);
#   * a runtime oracle that links the shipping zone allocator
#     (code/qcommon/memory.c) under ASan and drives the two pools the client
#     binds, so "the boundary frees the arena and not the pool" is a measured
#     fact, not a reading.
# Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code"

cd "$here"

rc=0

if ! python3 ./arenabound_control.py 0; then
	echo "  FAIL  the shipping sources are not green"
	rc=1
fi

# ---- the runtime oracle: the two pools, driven through the shipping allocator
cc -DAPP_MODULE -DBOTLIB -DDEDICATED -DEF2_MULTITHREAD=1 -DNDEBUG -DNO_GZIP -D_LINUX=1 \
	-I"$src/qcommon" -I"$src/script" -I"$src/corepp" -I"$src" \
	-I"$src/thirdparty/zlib-1.3.1" \
	-std=gnu17 -O1 -g -fsanitize=address -fno-omit-frame-pointer \
	-o ./arenabound_oracle ./arenabound_oracle.c "$src/qcommon/memory.c"
trap 'rm -f ./arenabound_oracle' EXIT

if ASAN_OPTIONS=detect_leaks=0 ./arenabound_oracle 0; then
	echo "  ok    the boundary frees the arena and keeps the pool"
else
	echo "  FAIL  the boundary does not free the arena or does not keep the pool"
	rc=1
fi

for m in 1 2; do
	if ASAN_OPTIONS=detect_leaks=0 ./arenabound_oracle "$m" >/dev/null 2>&1; then
		echo "  FAIL  runtime fault $m was not detected"
		rc=1
	else
		echo "  ok    runtime fault $m detected"
	fi
done

# ---- the source-invariant checker
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
trap 'rm -f ./arenabound_oracle; rm -rf "$mut"' EXIT

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
