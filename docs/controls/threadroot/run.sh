#!/bin/sh
# The walk's job dispatch, measured: the shipping q_jobs.c pool against the
# pool it replaced, driven through the shapes the walk submits.  The control
# must show the lock/futex thrash the device profile shows and the shipping
# pool must remove it, and the two must write the same bytes.  Never launches
# the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code"

cd "$here"

gcc -std=gnu17 -O2 -g -Wall -Werror -DEF2_MULTITHREAD=1 \
	-o ./threadroot_oracle threadroot_oracle.c "$src/qcommon/q_jobs.c" \
	-I"$src/qcommon" -I"$src" $( pkg-config --cflags --libs sdl2 )

trap 'rm -f threadroot_oracle' EXIT

rc=0

for w in "$@"; do
	if ! ./threadroot_oracle "$w"; then
		echo "  FAIL  the rig is red at $w workers"
		rc=1
	fi
done

if [ "$#" -eq 0 ]; then
	./threadroot_oracle || rc=1
fi

exit $rc
