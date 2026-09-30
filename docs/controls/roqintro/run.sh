#!/bin/sh
# The intro RoQ chain's offline control: green on the shipping tree, red on a
# planted fault at every link (the exec, its order, the cinematic command, the
# dointro queue, the shipped introCommand, the binaries' string table).
# Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
cd "$here"

python3 ./roq_intro_control.py
rc=0

for m in 1 2 3 4 5; do
	if python3 ./roq_intro_control.py "$m" >/dev/null 2>&1; then
		echo "  FAIL  planted fault $m was not detected"
		rc=1
	else
		echo "  ok    planted fault $m detected"
	fi
done

exit $rc
