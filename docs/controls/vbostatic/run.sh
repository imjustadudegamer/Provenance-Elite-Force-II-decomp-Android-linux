#!/bin/sh
# The static device-buffer path's offline oracle: green on the shipping
# vk_vbo.c, red on every planted fault.  Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code"

cd "$here"

gcc -std=gnu17 -O1 -g -o ./vbostatic_oracle vbostatic_oracle.c \
	-I"$src/renderervulkan" -I"$src/qcommon" -I"$src/renderercommon" -I"$src" \
	-I"$src/thirdparty/jpeg-9f" $( pkg-config --cflags sdl2 )

./vbostatic_oracle 0
rc=0

for m in 1 2 3 4 5 6 7; do
	if ./vbostatic_oracle "$m" >/dev/null 2>&1; then
		echo "  FAIL  planted fault $m was not detected"
		rc=1
	else
		echo "  ok    planted fault $m detected"
	fi
done

rm -f ./vbostatic_oracle
exit $rc
