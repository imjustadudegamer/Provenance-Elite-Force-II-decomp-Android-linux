#!/usr/bin/env bash
# ProvenanceEF2 Android APK build (the device target).
#
# Usage:   app/build_apk.sh [assembleDebug|assembleRelease|installDebug] ...
# Default: assembleDebug
#
# The heavy lifting is the engine's own CMake build (app/src/main/cpp), driven by
# gradle's externalNativeBuild with the NDK toolchain; this wrapper sets the
# environment gradle needs and runs it in the foreground.
#
# Requires: an Android SDK (local.properties, or ANDROID_SDK_ROOT) with the
# platform + build-tools + the pinned NDK, and a JDK 17+.
set -euo pipefail

PROJ="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$PROJ/.." && pwd)"

# Tree isolation: never let a build absorb an untracked or modified file the engine's
# CMake globs would compile straight into the APK.  The guard fails here instead.
# Deliberate override: EF2_TREE_DIRTY_OK=1
"$ROOT/personal/scripts/check_tree_isolation.sh" --build || exit $?

export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Android/sdk}"
export ANDROID_HOME="$ANDROID_SDK_ROOT"
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-21-openjdk-amd64}"

#
# Keep Gradle's and ccache's state inside the tree when $HOME cannot be written.
#
# Both default to $HOME (.gradle / .cache/ccache).  A sandboxed shell is refused
# there ("Read-only file system") and Gradle dies on its wrapper lock before it
# builds anything -- the failure is `gradle-8.13-bin.zip.lck ... Read-only file
# system`, not a build error.  When $HOME is not writable we fall back to copies
# kept under personal/evidence/ (gitignored), so the build completes with no privilege
# escalation at all.  A normal interactive shell has a writable $HOME and is
# completely unaffected -- no path here changes for it.
#
if [ ! -w "${HOME}/.gradle" ] && [ -d "${ROOT}/personal/evidence/gradle-home/wrapper" ]; then
    export GRADLE_USER_HOME="${GRADLE_USER_HOME:-${ROOT}/personal/evidence/gradle-home}"
    echo "note: \$HOME/.gradle is not writable -> GRADLE_USER_HOME=${GRADLE_USER_HOME}"
fi
if [ ! -w "${HOME}/.cache" ] && [ -d "${ROOT}/personal/evidence/ccache" ]; then
    export CCACHE_DIR="${CCACHE_DIR:-${ROOT}/personal/evidence/ccache}"
    echo "note: \$HOME/.cache is not writable -> CCACHE_DIR=${CCACHE_DIR}"
fi
# AGP keeps its analytics/prefs under $HOME/.android and WARNS when that is not
# writable.  Point it at a writable directory inside the tree instead.
# NOTE: do NOT also set ANDROID_PREFS_ROOT.  AGP 8.13 rejects the legacy variable:
# it fails to construct AndroidLocationsBuildService and the whole configure dies.
if [ ! -w "${HOME}/.android" ]; then
    export ANDROID_USER_HOME="${ANDROID_USER_HOME:-${ROOT}/personal/evidence/android-home}"
    mkdir -p "$ANDROID_USER_HOME"
    echo "note: \$HOME/.android is not writable -> ANDROID_USER_HOME=${ANDROID_USER_HOME}"
fi

cd "$ROOT"
./gradlew "${@:-assembleDebug}"
