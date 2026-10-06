#!/bin/sh
# The 2026-10-20 mission-failure disconnect killed the engine 190 rows *after* the
# error that looked like the cause: Com_Error(ERR_DISCONNECT, "") longjmp'd cleanly,
# the menu came up, and R_AddDrawSurf then faulted on a NULL shader whose
# tr.shaders[] entry had died with the registration.  This control holds the
# disconnect-into-3D path to the invariants that close that, and plants one fault
# per invariant to prove it can detect them.  Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
cd "$here"
code=$( cd ../../../app/src/main/cpp/code && pwd )

SDL=$( pkg-config --cflags-only-I sdl2 2>/dev/null || echo -I/usr/include/SDL2 )

gcc -std=gnu17 -O2 -I"$code/.." -I"$code" $SDL \
	-o ./disconnect3d_oracle disconnect3d_oracle.c

./disconnect3d_oracle
rc=0
m=1
while [ "$m" -le 9 ]; do
	if ./disconnect3d_oracle "$m" >/dev/null 2>&1; then
		echo "  FAIL  planted fault $m was not detected"
		rc=1
	else
		echo "  ok    planted fault $m detected"
	fi
	m=$(( m + 1 ))
done
rm -f ./disconnect3d_oracle

if [ "$rc" -eq 0 ]; then
	echo "disconnect3d control: GREEN"
fi
exit $rc
