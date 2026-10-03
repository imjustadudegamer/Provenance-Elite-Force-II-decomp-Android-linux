#!/bin/sh
# Command-pool ownership: one pool per producer, one lock per pool.  RED when a
# pool is reached from a second thread, when a second lock guards it, when a
# created pool has no lock, or when a transfer path bypasses the owner API.
# Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )
src="$here/../../../app/src/main/cpp/code/renderervulkan"

cd "$here"

rc=0

if ! python3 ./poolowner_control.py 0; then
	echo "  FAIL  the shipping call sites are not green"
	rc=1
fi

for m in 1 2 3 4 5; do
	if python3 ./poolowner_control.py "$m" >/dev/null 2>&1; then
		echo "  FAIL  planted fault $m was not detected"
		rc=1
	else
		echo "  ok    planted fault $m detected"
	fi
done

# source faults: the same breakages in the shipping text
mut=$( mktemp -d )
trap 'rm -rf "$mut"' EXIT

src_fault() {
	# $1 = the fault's name; the mutated copies are the arguments after
	shift
	python3 ./poolowner_control.py 0 "$@" >/dev/null 2>&1 && {
		echo "  FAIL  source fault $name was not detected"
		rc=1
	} || echo "  ok    source fault $name detected"
}

# 6: the arena's transfer buffer goes back to a shared pool
sed 's/allocInfo.commandPool = VK_Upload_Pool();/allocInfo.commandPool = vk.commandPool;/' \
	"$src/vk_vbo.c" > "$mut/vk_vbo.c"
name="6 the arena transfer leaves the walk's own pool"
src_fault "$name" "$src/vk_renderthread.c" "$mut/vk_vbo.c"

# 7: a pool is registered without a lock
sed 's/slot->lock = SDL_CreateMutex();/slot->lock = NULL;/' \
	"$src/vk_renderthread.c" > "$mut/vk_renderthread.c"
name="7 the pool has no lock"
src_fault "$name" "$mut/vk_renderthread.c"

# 8: the frame pool is destroyed after the device is brought down
sed 's/^\tVK_Pool_ShutdownAll();$//' "$src/vk_frame.c" > "$mut/vk_frame.c"
name="8 the pools are never destroyed"
src_fault "$name" "$src/vk_renderthread.c" "$src/vk_vbo.c" "$mut/vk_frame.c"

# 9: a one-shot transfer bypasses the owner API
sed 's/^\tVK_Upload_Begin();$/\tqvk.AllocateCommandBuffers( vk.device, \&allocInfo, \&cmd );\n\tVK_Upload_Begin();/' \
	"$src/vk_texture.c" > "$mut/vk_texture.c"
name="9 a transfer bypasses the owner API"
src_fault "$name" "$src/vk_renderthread.c" "$src/vk_vbo.c" "$src/vk_frame.c" "$mut/vk_texture.c"

exit $rc
