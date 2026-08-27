#!/usr/bin/env bash
#
# check_tree_isolation.sh -- the tree-isolation guard.
#
# THE RULE: there is exactly one main tree, and a build in it must not absorb
# uncommitted or untracked work the engine's CMake globs would compile into the
# shipped binary (cmake/basegame.cmake globs code/fgame/*.cpp, cmake/client.cmake
# globs code/uilib/*.c[pp], ...).  `git status`, `git add` and every tree census
# otherwise describe more than one writer at once.
#
# WHAT IT CHECKS
#   1. exactly one tree is registered (the project root);
#   2. the build inputs (app/src/main/cpp, app, docs/scripts) carry no uncommitted
#      or untracked work.  Ignored build output is fine -- only something that
#      would compile into the artefact counts.
#
# USAGE
#   docs/scripts/check_tree_isolation.sh              # audit the tree
#   docs/scripts/check_tree_isolation.sh --build      # same, naming the build refusal
#   docs/scripts/check_tree_isolation.sh --self-test  # plant a file, require RED,
#                                                     # remove it, require GREEN
#
# EXIT: 0 clean; 3 the tree carries work; 4 the tree count is wrong.
# Escape hatch for a deliberate dirty build: EF2_TREE_DIRTY_OK=1
#
set -uo pipefail

SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SELF_DIR/../.." && pwd)"

MODE="${1:-}"
case "$MODE" in
    ""|--build|--self-test) ;;
    *) echo "usage: $0 [--build|--self-test]" >&2; exit 2 ;;
esac

SCOPE=(app/src/main/cpp app docs/scripts)
PROBE_REL="app/src/main/cpp/code/qcommon/__tree_isolation_probe.tmp"

tree_count() {
    git -C "$ROOT" worktree list --porcelain 2>/dev/null | grep -c '^worktree '
}

dirty_in() {
    git -C "$ROOT" status --porcelain --untracked-files=all -- "${SCOPE[@]}" 2>/dev/null
}

# check -> 0 clean, 3 the tree carries work, 4 the tree count is wrong
check() {
    local build="${1:-}" rc=0 n d
    n="$(tree_count)"
    if [ "$n" != "1" ]; then
        # A second worktree is normal while a pass is running elsewhere.  What
        # actually matters is that no OTHER tree sits inside this tree's build
        # inputs, because the CMake globs would compile its files instead.
        local other_inside=0 wt
        while read -r wt; do
            [ "$wt" = "$ROOT" ] && continue
            case "$wt" in
                "$ROOT"/app|"$ROOT"/app/*|"$ROOT"/docs/scripts|"$ROOT"/docs/scripts/*) other_inside=1 ;;
            esac
        done <<EOF
$(git -C "$ROOT" worktree list --porcelain | awk '/^worktree /{print $2}')
EOF
        if [ "$other_inside" = "1" ]; then
            echo "check_tree_isolation: FAIL -- a worktree lies inside this tree's build inputs"
            git -C "$ROOT" worktree list | sed 's/^/    /'
            rc=4
        else
            echo "check_tree_isolation: NOTE -- $n trees registered; none inside this tree's build inputs"
        fi
    fi
    d="$(dirty_in)"
    if [ -n "$d" ]; then
        if [ "${EF2_TREE_DIRTY_OK:-0}" = "1" ]; then
            echo "check_tree_isolation: WARNING -- the tree carries work; overriding on EF2_TREE_DIRTY_OK=1"
            printf '%s\n' "$d" | sed 's/^/    /'
            rc=0
        else
            if [ "$build" = "--build" ]; then
                echo "check_tree_isolation: FAIL -- refusing to build: the tree carries work"
            else
                echo "check_tree_isolation: FAIL -- the tree carries work"
            fi
            printf '%s\n' "$d" | sed 's/^/    /'
            echo "    The engine's CMake globs compile untracked files here into the shipped binary."
            echo "    Commit, stash or remove the work first.  Deliberate override: EF2_TREE_DIRTY_OK=1"
            rc=3
        fi
    fi
    return "$rc"
}

if [ "$MODE" = "--self-test" ]; then
    # The override would mask the fault, so the self-test unsets it: an arm
    # that cannot fire proves nothing.
    unset EF2_TREE_DIRTY_OK
    probe="$ROOT/$PROBE_REL"
    cleanup() { rm -f "$probe"; }
    trap cleanup EXIT INT TERM

    check >/dev/null 2>&1; rc_clean=$?
    mkdir -p "$(dirname "$probe")"
    printf 'by check_tree_isolation.sh --self-test\n' > "$probe"
    check >/dev/null 2>&1; rc_dirty=$?
    rm -f "$probe"
    check >/dev/null 2>&1; rc_after=$?

    if [ "$rc_clean" -ne 0 ]; then
        echo "check_tree_isolation --self-test: FAIL -- the tree was already dirty before the plant (rc=$rc_clean)"
        exit 1
    fi
    if [ "$rc_dirty" -ne 3 ]; then
        echo "check_tree_isolation --self-test: FAIL -- $PROBE_REL and the guard returned $rc_dirty, expected 3: it cannot go RED, so a green run proves nothing"
        exit 1
    fi
    if [ "$rc_after" -ne 0 ]; then
        echo "check_tree_isolation --self-test: FAIL -- the guard stayed RED ($rc_after) after the plant was removed"
        exit 1
    fi
    echo "check_tree_isolation --self-test: PASS -- $PROBE_REL -> RED (3); removed -> GREEN (0)"
    exit 0
fi

check "$MODE"; rc=$?

if [ "$rc" -eq 0 ]; then
    echo "check_tree_isolation: OK -- one tree ($ROOT, $(git -C "$ROOT" rev-parse --short HEAD)), no work in the build inputs"
    exit 0
fi

if [ "$rc" -eq 3 ] && [ "${EF2_TREE_DIRTY_OK:-0}" = "1" ]; then
    echo "check_tree_isolation: WARNING -- the tree carries work, overriding on EF2_TREE_DIRTY_OK=1"
    exit 0
fi
exit "$rc"
