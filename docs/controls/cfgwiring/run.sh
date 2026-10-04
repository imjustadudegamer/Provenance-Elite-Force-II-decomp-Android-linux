#!/bin/sh
# The config wiring: which value the engine actually loads, and what the
# launcher's per-boot override does to it.  RED when the override exec leaves
# its slot in Com_Init, when the archive stops honouring setx, when the command
# line stops setting with force, when the launcher's block no longer fits the
# command line's 32 arguments, or when the override stops beating the config.
#
# Two checkers over the same class:
#   * a source-invariant checker over the shipping tree and the launcher on
#     disk (the slot cap model, the precedence model, the override file);
#   * an oracle that links the shipping cvar.c/cmd.c/memory.c and drives the
#     real dispatcher, so "a LATCHed set after registration does not bind" and
#     "the archive skips a setx cvar" are measured facts, not readings.
# Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code"

cd "$here"

rc=0

# the launcher lives outside the repo; audit the real one when it is there.
# From a worktree the repo root is two levels above the tree holding us.
tree_root=$( cd "$here/../../.." && pwd )
# the launcher lives outside the repo: beside this tree, or beside the worktree
for base in "$tree_root" "$tree_root/../../.."; do
	cand=$( cd "$base" 2>/dev/null && pwd )/personal/scripts/play_ef2_decomp.sh
	if [ -f "$cand" ]; then
		EF2_LAUNCHER_PATH="$cand"
		export EF2_LAUNCHER_PATH
		break
	fi
done

if python3 ./cfgwiring_control.py 0; then
	echo "  ok    the launch/config chain is wired"
else
	echo "  FAIL  the launch/config chain is not wired"
	rc=1
fi

# ---- the oracle: the cvar resolution, driven through the shipping code
cc -DSTANDALONE -DDEDICATED -DNDEBUG -D_LINUX=1 \
	-I"$src/qcommon" -I"$src" \
	-std=gnu17 -O1 -g -fsanitize=address -fno-omit-frame-pointer \
	-o ./cfgwiring_oracle ./cfgwiring_oracle.c \
	"$src/qcommon/cvar.c" "$src/qcommon/cmd.c" "$src/qcommon/memory.c" \
	"$src/qcommon/q_shared.c" -lm
trap 'rm -f ./cfgwiring_oracle' EXIT

if ASAN_OPTIONS=detect_leaks=0 ./cfgwiring_oracle; then
	echo "  ok    the cvar resolution binds, latches and archives as the override needs"
else
	echo "  FAIL  the cvar resolution does not do what the override needs"
	rc=1
fi

# ---- source faults: the same breakages in the shipping text
for m in 1 2 3 4 5; do
	if python3 ./cfgwiring_control.py "$m" >/dev/null 2>&1; then
		echo "  FAIL  source fault $m was not detected"
		rc=1
	else
		echo "  ok    source fault $m detected"
	fi
done

# ---- launcher faults: the override defeated, the archive bleeding, the block
#      too big for the command line
for m in 6 7 8 9; do
	if python3 ./cfgwiring_control.py "$m" >/dev/null 2>&1; then
		echo "  FAIL  launcher fault $m was not detected"
		rc=1
	else
		echo "  ok    launcher fault $m detected"
	fi
done

exit $rc
