#!/bin/sh
#
# ninja-guard.sh -- run ninja, then detect the one staleness failure ninja
# structurally cannot see.
#
# WHY THIS EXISTS
#
# Ninja's header tracking via depfiles is correct: change a header, and every
# object whose depfile names it is rebuilt.  That was measured, not assumed --
# `touch code/fgame/entity.h code/fgame/mover.h` rebuilds 107 objects, and a
# `git merge` that re-parents Mover/ScriptSlave/Gib followed by `ninja -C build`
# rebuilds 109 and links clean.
#
# What ninja cannot see is a source file changing *while the build is running*.
# Ninja stats an edge's inputs when it schedules that edge.  If the tree is
# mutated mid-build -- a `git merge`, `git checkout`, `git stash`, an editor
# write, or a second ninja process in the same build directory -- then:
#
#   * objects whose compile had ALREADY STARTED read the pre-change headers,
#   * but their .o is WRITTEN when the compile finishes, i.e. AFTER the change,
#   * so the .o mtime is newer than the header it disagrees with, and
#   * ninja will consider that object up to date FOREVER.
#
# Under -flto the linker then merges objects that disagree about class layout.
# Measured on this tree: 78 `-Wodr` + 15 `-Wlto-type-mismatch` on the first
# link; a second plain `ninja` rebuilt 74 objects and STILL emitted 108 -Wodr;
# a third said "no work to do".  `ninja -t clean` + rebuild of the identical
# source: 0 warnings.  The corruption is silent, sticky, and survives any
# number of incremental builds.
#
# The only sound repair is `ninja -t clean`.  This script makes the condition
# loud instead of silent, and can repair it automatically.
#
# USAGE
#   misc/ninja-guard.sh [-C <builddir>] [-a] [ninja args...]
#
#   -C <builddir>   build directory (default: build)
#   -a              on detection, run `ninja -t clean` and rebuild automatically
#                   (same as NINJA_GUARD_AUTOCLEAN=1)
#
# Exit status is ninja's, except that a detected mid-build mutation forces a
# non-zero exit when -a was not given.
#

set -eu

BUILDDIR=build
AUTOCLEAN=${NINJA_GUARD_AUTOCLEAN:-0}
NINJA=${NINJA:-ninja}

while [ $# -gt 0 ]; do
    case "$1" in
        -C) BUILDDIR=$2; shift 2 ;;
        -C*) BUILDDIR=${1#-C}; shift ;;
        -a) AUTOCLEAN=1; shift ;;
        --) shift; break ;;
        *) break ;;
    esac
done

if [ ! -f "$BUILDDIR/build.ninja" ]; then
    echo "ninja-guard: no build.ninja in '$BUILDDIR'" >&2
    exit 2
fi

SRCDIR=$(cd "$(dirname "$0")/.." && pwd)
CODEDIR=$SRCDIR/code
MARKER=$BUILDDIR/.ninja-guard.start
LOCK=$BUILDDIR/.ninja-guard.lock

# Files the BUILD itself writes back into the source tree (flex/bison output).
# These are legitimate mid-build source changes and must not be flagged.
PRUNE="-path $CODEDIR/parser/generated -prune -o"

run_ninja() {
    # Serialise concurrent ninja invocations on the same build directory.
    # Two ninja processes racing in one build dir produce exactly the mixed
    # object set described above, and ninja takes no lock of its own.
    if command -v flock >/dev/null 2>&1; then
        flock "$LOCK" "$NINJA" -C "$BUILDDIR" "$@"
    else
        "$NINJA" -C "$BUILDDIR" "$@"
    fi
}

: > "$LOCK"
# The marker is created immediately before ninja starts, so edits made BEFORE
# the build (the normal case) are not flagged -- only edits that land while
# ninja is running.
touch "$MARKER"

set +e
run_ninja "$@"
STATUS=$?
set -e

CHANGED=$(find "$CODEDIR" $PRUNE -type f -newer "$MARKER" -print 2>/dev/null | head -40)

if [ -n "$CHANGED" ]; then
    echo >&2
    echo "############################################################" >&2
    echo "ninja-guard: THE SOURCE TREE CHANGED WHILE THIS BUILD RAN." >&2
    echo >&2
    echo "Objects whose compile was already in flight were built against" >&2
    echo "the PREVIOUS content of these files, yet their .o mtime is now" >&2
    echo "newer than the files themselves.  Ninja will never rebuild them." >&2
    echo "Under -flto the link silently merges disagreeing class layouts." >&2
    echo >&2
    echo "Files that changed during the build:" >&2
    echo "$CHANGED" | sed 's/^/  /' >&2
    echo >&2
    echo "ANY warning count from this build is meaningless, and so is the" >&2
    echo "binary.  The only sound repair is:" >&2
    echo "    ninja -C $BUILDDIR -t clean && ninja -C $BUILDDIR" >&2
    echo "############################################################" >&2
    echo >&2

    if [ "$AUTOCLEAN" = "1" ]; then
        echo "ninja-guard: -a given; cleaning and rebuilding." >&2
        touch "$MARKER"
        set +e
        run_ninja -t clean >/dev/null
        run_ninja "$@"
        STATUS=$?
        set -e
        CHANGED=$(find "$CODEDIR" $PRUNE -type f -newer "$MARKER" -print 2>/dev/null | head -40)
        if [ -n "$CHANGED" ]; then
            echo "ninja-guard: the tree changed AGAIN during the repair build." >&2
            echo "Stop mutating the tree while a build is running." >&2
            exit 1
        fi
    else
        exit 1
    fi
fi

rm -f "$MARKER"
exit $STATUS
