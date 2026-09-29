#!/bin/sh
# The walk's job-ized pack: green on the shipping vk_shade.c body driven
# through the shipping q_jobs.c pool, red on every planted fault -- in the
# body's call pattern and in the source texts gen_pack.py lifts out of it.
# Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code"

cd "$here"

python3 ./gen_pack.py "$src/renderervulkan/vk_shade.c" \
	"$src/renderervulkan/tr_vbo.h" ./jobwalk_pack_gen.c

gcc -std=gnu17 -O2 -g -Wall -Werror -DEF2_MULTITHREAD=1 \
	-o ./jobwalk_oracle jobwalk_oracle.c "$src/qcommon/q_jobs.c" \
	-I"$src/qcommon" -I"$src" $( pkg-config --cflags --libs sdl2 )

rc=0

if ! ./jobwalk_oracle 0; then
	echo "  FAIL  the shipping pack is not green"
	rc=1
fi

for m in 1 2 3 4 5 6; do
	if ./jobwalk_oracle "$m" >/dev/null 2>&1; then
		echo "  FAIL  planted fault $m was not detected"
		rc=1
	else
		echo "  ok    planted fault $m detected"
	fi
done

# source-level faults: mutate the texts the control lifts and require RED
mut=$( mktemp -d )
trap 'rm -rf "$mut" jobwalk_oracle jobwalk_pack_gen.c' EXIT

mutate() {
	sed "$1" "$src/renderervulkan/vk_shade.c" > "$mut/vk_shade.c"
	if python3 ./gen_pack.py "$mut/vk_shade.c" "$src/renderervulkan/tr_vbo.h" \
		"$mut/gen.c" >/dev/null 2>&1; then
		echo "  FAIL  source fault $2 was not detected"
		rc=1
	else
		echo "  ok    source fault $2 detected"
	fi
}

mutate 's/JobPool_ParallelRange( input->numVertexes, VK_PACK_CHUNK, VK_Stream_PackRange, &packCtx );/for ( i = 0; i < input->numVertexes; i++ ) {}/' \
	"7 the dispatch is gone"
mutate 's/^#define VK_PACK_CHUNK\t256$/#undef VK_PACK_CHUNK/' \
	"8 the chunk define is gone"
mutate 's/out->xyz\[0\] = xyz\[0\];/out->xyz[0] = input->shader != NULL;/' \
	"9 the pack reads more of the tess store"

exit $rc
