#!/bin/sh
# The 2D window, the widget's placement and the writer census.  RED when:
#   1  the frame boundary does not drop the 2D window cache
#   2  a 3D command does not drop it
#   3  the walk thread drops it (the race that flashes the 2D)
#   4  a widget passes its own frame as the window's ortho again
#   5  a widget stops publishing its placement
#   6  the recorder does not drop the placement on the elided path
#   7  the placement y is the GL-viewport box again (the mirror)
#   8  a font/glyph writer bypasses the placement (the menu's missing text)
#   9  the glyph placement lands before the width/height scale
#   10 the single placement site loses the offset
#   11 the placement loses the clipped origin
#   12 an immediate-stream writer bypasses the placement
# Never launches.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
cd "$here"

python3 ./d2window_control.py
rc=0

for m in 1 2 3 4 5 6 7 8 9 10 11 12; do
	if python3 ./d2window_control.py "$m" >/dev/null 2>&1; then
		echo "  FAIL  planted fault $m was not detected"
		rc=1
	else
		echo "  ok    planted fault $m detected"
	fi
done

exit $rc
