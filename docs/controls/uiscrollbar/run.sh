#!/bin/sh
# The SCROLLBAR WIDTH control: green on the shipping tree -- every
# scrollbar_width the shipped data declares is honoured end to end and the four
# retail functions the chain is transcribed from agree field for field -- and
# red on every planted fault.  Never launches; the retail decompiles are the
# authority for the chain, the shipped .urc for the authored widths.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
cd "$here"

rc=0

if ! python3 ./uiscrollbar_control.py 0; then
	echo "  FAIL  the shipping tree is not green"
	rc=1
fi

for m in 1 2 3 4 5 6; do
	if python3 ./uiscrollbar_control.py "$m" >/dev/null 2>&1; then
		echo "  FAIL  model fault $m was not detected"
		rc=1
	else
		echo "  ok    model fault $m detected"
	fi
done

exit $rc
