#!/bin/sh
# The beam/radar colour control.  The two additive surfaces the operator named --
# the borg overhead light's volumetric beam and the HUD radar fov cone -- are
# pinned on both sides of the sampler: the shipped DDS colour blocks (decoded
# with this lane's own DXT3 algorithm), the shipped shader text that authors each
# combine, and the shipping source's generator and blend-state constants with the
# vertex-shader arms those constants select, plus the stage-uniform publish
# (every vec4 whole, not just its red channel) and the flat gate's inert-at-0
# ordering.  Green on the shipping tree and the shipped art, red on every
# planted fault.  Never launches.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code"

cd "$here"

rc=0

if ! python3 ./beamradar_control.py; then
	echo "  FAIL  the shipping tree and the shipped data are not the retail colours"
	rc=1
fi

for m in 1 2 3 4 5 6 7 8; do
	if python3 ./beamradar_control.py --mutate "$m" >/dev/null 2>&1; then
		echo "  FAIL  model fault $m was not detected"
		rc=1
	else
		echo "  ok    model fault $m detected"
	fi
done

exit $rc
