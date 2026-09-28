#!/bin/sh
# What the footprint census costs per frame, and the default-off gate that stops
# the walk paying it for a row that prints nothing.  Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code"

cd "$here"
gcc -std=gnu17 -O2 -o ./footprint_oracle footprint_oracle.c

./footprint_oracle "$src/renderervulkan/vk_shade.c" 0 12000
rc=0
for m in 1; do
	if ./footprint_oracle "$src/renderervulkan/vk_shade.c" "$m" 12000 >/dev/null 2>&1; then
		echo "  FAIL  planted fault $m was not detected"
		rc=1
	else
		echo "  ok    planted fault $m detected"
	fi
done
rm -f ./footprint_oracle
exit $rc
