#!/bin/sh
# The SAVE-GAME SCREENSHOT control: green on the shipping tree -- the
# `screenshot` command registered, the request answered at the present seam,
# the TGA retail's shape, the resampler's indices inside the window and running
# bottom-up to top -- and red on every planted fault.  Never launches; the
# retail corpus's own save screenshots are the shape oracle.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
cd "$here"

rc=0

if ! python3 ./saveshot_control.py 0; then
	echo "  FAIL  the shipping tree is not green"
	rc=1
fi

for m in 1 2 3 4 5 6; do
	if python3 ./saveshot_control.py "$m" >/dev/null 2>&1; then
		echo "  FAIL  model fault $m was not detected"
		rc=1
	else
		echo "  ok    model fault $m detected"
	fi
done

exit $rc
