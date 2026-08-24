#!/bin/sh
#
# odr-race-repro.sh -- self-contained, ~20 second proof that ninja cannot see a
# source change that lands WHILE a build is running, and that the resulting
# stale object is accepted forever afterwards.
#
# This is the mechanism behind the 71 `-Wodr` warnings seen on an incremental
# build of this tree in wave 6 that a `ninja -t clean` + rebuild of identical
# source did not reproduce.  It is NOT a depfile bug: ninja's header tracking
# is correct (see docs/KNOWN_GROUND.md, "BUILD INTEGRITY").
#
# Ninja stats an edge's inputs when it SCHEDULES that edge.  Here a.o's compile
# reads base.h at t=0, the header is rewritten at t=3, and a.o is written at
# t=8 -- so a.o holds the OLD layout but has a NEWER mtime than the header it
# disagrees with.  b.o and main.o then compile at t=8 with the NEW layout.  The
# -flto link merges both.  Every subsequent `ninja` says "no work to do".
#
# Expected output:
#   race run          : 2 warnings, both [-Wodr]
#   2nd ninja         : ninja: no work to do.     <-- STICKY
#   3rd ninja         : ninja: no work to do.
#   ninja -t clean    : 0 warnings
#
# Usage: misc/odr-race-repro.sh [workdir]     (default: a mktemp -d)

set -eu

DIR=${1:-$(mktemp -d)}
mkdir -p "$DIR"
cd "$DIR"

printf 'struct B { int x; };\nstruct D : B { int y; };\n' > base.h
printf '#include "base.h"\nD ga;\nint fa(D *d) { return d->y + d->x; }\n' > a.cpp
printf '#include "base.h"\nextern D ga;\nint fb(D *d) { return d->y - d->x; }\n' > b.cpp
printf '#include "base.h"\nint fa(D *);\nint fb(D *);\nextern D ga;\nint main() { return fa(&ga) + fb(&ga); }\n' > main.cpp

# `sleep $delay && touch $out` models a slow compile: the headers are read at
# the start, the object materialises at the end.  Real compiles in this tree
# take 2-20 s, which is the whole width of the hazard window.
cat > build.ninja <<'EOF'
rule cxx
  command = g++ -O2 -flto -c $in -o $out -MMD -MF $out.d && sleep $delay && touch $out
  depfile = $out.d
  deps = gcc
rule link
  command = g++ -O2 -flto $in -o $out
build a.o: cxx a.cpp
  delay = 8
build b.o: cxx b.cpp
  delay = 0
build main.o: cxx main.cpp
  delay = 0
build prog: link a.o b.o main.o
default prog
EOF

echo "== baseline build =="
ninja -j1 >/dev/null 2>&1
echo "   ok, 0 warnings"

echo
echo "== race: rebuild everything, rewrite base.h 3 s in =="
touch a.cpp b.cpp main.cpp
( ninja -j1 > race.log 2>&1 ) &
NPID=$!
sleep 3
printf 'struct B { int x; };\nstruct Z { long q; };\nstruct D : B, Z { int y; };\n' > base.h
wait $NPID || true
echo "   -Wodr warnings at link: $(grep -c -- '-Wodr' race.log || true)"
grep 'warning:' race.log | sed 's/^/   /' | head -4 || true

echo
echo "== is it sticky? =="
echo "   2nd ninja: $(ninja -j1 2>&1 | tail -1)"
echo "   3rd ninja: $(ninja -j1 2>&1 | tail -1)"
echo "   base.h  mtime: $(stat -c '%.3Y' base.h)"
echo "   a.o     mtime: $(stat -c '%.3Y' a.o)   <-- newer than the header it disagrees with"

echo
echo "== the only repair =="
ninja -t clean >/dev/null
echo "   ninja -t clean && ninja  ->  warnings: $(ninja -j1 2>&1 | grep -c 'warning:' || true)"

echo
echo "workdir: $DIR"
