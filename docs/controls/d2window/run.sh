#!/bin/sh
# The 2D window/scissor elision cache: owned by the front end, dropped at the
# frame boundary and after every 3D command.  RED when the drop lives on the
# walk thread (the race that flashes the 2D) or is missing.  Never launches.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
cd "$here"

python3 ./d2window_control.py
rc=0

for m in 1 2 3; do
	if python3 ./d2window_control.py "$m" >/dev/null 2>&1; then
		echo "  FAIL  planted fault $m was not detected"
		rc=1
	else
		echo "  ok    planted fault $m detected"
	fi
done

exit $rc
