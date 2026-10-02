#!/usr/bin/env python3
"""
walkaccount_control.py -- the walk split closes inside one drain.

The `[PERF:walk]` row splits the walk thread's whole body into named buckets and
prints `rest=` as the amount the buckets did not claim.  `rest=` is therefore the
instrument's own error, and the row says so; a residual that moves with the
frame is the accounting and not a cost.

The buckets BIND/REC/PACK are charged at their own sites, and every window that
contains those sites subtracts them again so no time is counted twice.  That
subtraction is `Nested() - n0`, where the three counters are cumulative since the
last drain -- and the front end drains them at a 1 s cadence, from the other
thread.  A drain landing inside a window restarts the counters, so the
difference goes negative and the window's "subtract the nested part" becomes an
addition of the stale count: the bucket is inflated by an amount that never
happened, and `rest=` reads large and negative.

The shipping rule is `NestedSince(n0)`: after a drain the counter holds exactly
what was added since it, so that reading is the window's contribution.  This
control models both rules over a window with a drain inside it, and censuses the
shipping text for the rule and its users.

Planted faults break each in turn (argv[1]); argv[2:] override the sources.

Build/run: docs/controls/walkaccount/run.sh  (never launches the game)
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
WT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
VK = os.path.join(WT, "app", "src", "main", "cpp", "code", "renderervulkan")

DEFAULT_FILES = {
    "local": os.path.join(VK, "vk_local.h"),
    "cmds": os.path.join(VK, "tr_cmds.c"),
    "backend": os.path.join(VK, "tr_backend.c"),
}

failures = 0
checks = 0


def check(cond, msg):
    global failures, checks
    checks += 1
    if not cond:
        failures += 1
        print("  FAIL  " + msg)


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def model(nested_shipping, drain_inside):
    """One walk window that adds `nested` of BIND/REC/PACK and `own` of its own
    time.  With drain_inside the front end drains the counters mid-window.

    Returns (window_charge, true_charge): the shipping rule must charge only the
    window's own time (the nested part is charged by its own leaves)."""
    n0 = 1000            # the cumulative nested counter at window open
    nested = 300         # what this window's leaves add
    own = 700            # the window's own, non-nested time
    if drain_inside:
        # the drain zeroes the counters, then this window's leaves add again
        after_drain = nested
    else:
        after_drain = n0 + nested
    if nested_shipping:
        n = after_drain
        net = n if n < n0 else n - n0
    else:
        net = after_drain - n0
    return own + nested - net, own


def main():
    mutant = int(sys.argv[1]) if len(sys.argv) > 1 else 0

    files = dict(DEFAULT_FILES)
    for i, key in enumerate(["local", "cmds", "backend"]):
        if len(sys.argv) > 2 + i:
            files[key] = sys.argv[2 + i]

    local = read(files["local"])
    cmds = read(files["cmds"])
    backend = read(files["backend"])

    nested_shipping = True

    if mutant == 1:
        # the macro goes back to the raw subtraction
        local = local.replace("VK_WalkStat_NestedSince( n0 );", "VK_WalkStat_Nested() - (n0);")
        nested_shipping = False
    elif mutant == 2:
        # the part/window charges go back to the raw subtraction
        backend = backend.replace("VK_WalkStat_NestedSince( nested0 )", "( VK_WalkStat_Nested() - nested0 )")
        nested_shipping = False
    elif mutant == 3:
        # the rule stops handling the drain
        cmds = cmds.replace("return ( n < n0 ) ? n : ( n - n0 );", "return n - n0;")
        nested_shipping = False

    # --- 1. the rule -------------------------------------------------------
    fn = cmds[cmds.find("long long VK_WalkStat_NestedSince"):]
    fn = fn[:fn.find("\n}\n") + 3] if "\n}\n" in fn else fn
    check(bool(fn), "VK_WalkStat_NestedSince is gone")
    if nested_shipping:
        check("n < n0" in fn, "the rule no longer handles a drain inside the window")
        check("? n :" in fn, "the rule does not fall back to the drained reading")
        check("n - n0" in fn, "the rule has no ordinary difference")

    # --- 2. every window uses the rule ------------------------------------
    check("VK_WalkStat_NestedSince( n0 )" in local,
          "VK_WALK_ADD_NET no longer uses the drain-safe nested reading")
    check("VK_WalkStat_Nested() - " not in local,
          "VK_WALK_ADD_NET still subtracts the raw cumulative counter")
    for name, text in (("VK_Shade_PartCharge", backend), ("VK_Shade_WindowClose", backend)):
        i = text.find(name)
        body = text[i:i + 700] if i >= 0 else ""
        check(bool(body), "%s is gone" % name)
        if nested_shipping:
            check("VK_WalkStat_NestedSince( nested0 )" in body,
                  "%s does not use the drain-safe nested reading" % name)
        check("VK_WalkStat_Nested() - nested0" not in body,
              "%s still subtracts the raw cumulative counter" % name)

    # --- 3. the model ------------------------------------------------------
    ship_clean, true_clean = model(nested_shipping, drain_inside=False)
    ship_drain, true_drain = model(nested_shipping, drain_inside=True)
    check(ship_clean == true_clean,
          "a window with no drain inside does not close (charged %d, owed %d)"
          % (ship_clean, true_clean))
    check(ship_drain == true_drain,
          "a window with a drain inside inflates (charged %d, owed %d)"
          % (ship_drain, true_drain))

    print("walkaccount control: %d checks, %d failures" % (checks, failures))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
