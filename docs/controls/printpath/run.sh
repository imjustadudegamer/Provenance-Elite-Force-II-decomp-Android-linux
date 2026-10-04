#!/bin/sh
# The console output path must not be able to go permanently silent.  The
# 2026-10-19 boots prove it can: both sinks stopped at the frame the operator
# clicked load and the engine kept rendering, so three runs of evidence were
# lost.  This control holds the print path to the invariants that close that
# class, and plants one fault per invariant to prove it can detect them.
# Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
cd "$here"
gcc -std=gnu17 -O2 -o ./printpath_oracle printpath_oracle.c

./printpath_oracle
rc=0
m=1
while [ "$m" -le 8 ]; do
	if ./printpath_oracle "$m" >/dev/null 2>&1; then
		echo "  FAIL  planted fault $m was not detected"
		rc=1
	else
		echo "  ok    planted fault $m detected"
	fi
	m=$(( m + 1 ))
done
rm -f ./printpath_oracle

if [ "$rc" -eq 0 ]; then
	echo "printpath control: GREEN"
fi
exit $rc
