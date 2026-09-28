#!/bin/sh
# The dynamic-offset contract of every VK_Draw_BindSet call, against the count
# its own descriptor-set layout declares (VUID-...-dynamicOffsetCount-00359).
# RED when a call passes a count its layout does not declare.  Never launches.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
cd "$here"

python3 ./desccount_control.py
rc=0
for m in 1 2 3; do
	if python3 ./desccount_control.py "$m" >/dev/null 2>&1; then
		echo "  FAIL  planted fault $m was not detected"
		rc=1
	else
		echo "  ok    planted fault $m detected"
	fi
done
exit $rc
