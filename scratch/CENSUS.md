# r40 evidence census

Source branch `r40-menu-trace-perf` at `61ca5b39`.  Input evidence: the
operator's quiet boot at `~/.cache/ef2-android/logs/run-20261020-143451/`
(perf.data 8.1MB, threads.tsv, cores.tsv, gpu.tsv, game.log 24283 lines).

## 0. Scoping facts about the profile itself

These change how the brief's percentages must be read; all were measured from
the file, not assumed.

* **perf.data is main-thread only.**  Every one of the 68885 sample blocks
  carries tid `903226`:

      awk '!/^[ \t]/ && NF>=2 {print $2}' script.txt | sort | uniq -c
        68885 903226

  threads.tsv names the other threads (cljob0-5, `.:disk$0-3`, `.:traceq0`,
  `WSI swapchain q/e`, vkjob0-N), and none of them appears in the sample
  stream.  So the walk thread and the vkjob pool are absent: every percentage
  below is a share of the MAIN thread, which is where the frame's critical
  path runs.

* **Two events, both recorded.**  `perf report --header-only` reports a 30s
  window (185144.889 -> 185174.893) with `cpu_atom/cycles/P` (16K samples) and
  `cpu_core/cycles/P` (52K samples).  The brief's figures are the atom block;
  the core block is 3x larger and is where the work actually runs.  Both are
  tabulated in `attr_all.txt` and they rank the same clusters, so conclusions
  do not depend on which block is quoted.

* **The run was the pause menu over a live world.**  game.log's tail shows
  `menu/main/bkg_04`, `pauseicon`, `sysimg/fonts/...` 2D draws beside
  `textures/borg/bfloor2` and `*lightmap52` stage binds, ending in
  `Server Shutdown (Closed window)`.  The UI cluster is therefore real work,
  not boot-only.

* **The frame loop is uncapped.**  `[UI:cg] frames=259..559 window=1000ms`
  across the log, against `common.c:2827` (`minMsec = 1` when
  `com_maxfps <= 0`).  250-550 frames/s is why per-frame UI work is
  over-represented: everything is done 400 times a second.  Note this is the
  same regime where the archived `r_swapInterval` finding left every fps
  reading FIFO-capped, so the frame counts above are loop iterations, not a
  display-locked rate.

* **The build is `-flto`, and ICF folds distinct functions onto one address.**
  `cmake/compilers/gnu.cmake:64` documents `-flto -fno-fat-lto-objects`.
  Measured on the built `provenanceef2`:

      nm -C --defined-only provenanceef2 | awk '{print $1}' | sort | uniq -c \
        | awk '$1>1{n++; s+=$1} END{print n, s}'
      190 606

  i.e. 190 addresses carry 606 symbols.  A concrete instance, and one the
  profile actually trips over: `UIWidget::UpdateData()` and
  `UIWidget::UpdateUIElement()` both sit at `0x65010`.

  Consequence: **leaf self-time names are trustworthy** (each of the top
  symbols owns its address), but **caller names one frame up are not** -- the
  unwinder lands in a folded region and the symboliser picks one of the folded
  names.  That is exactly the `UIWidget::update <- UIWidgetContainer::
  ~UIWidgetContainer` chain the perf report prints (64.3% of that symbol's
  samples), and `Menu::ForceHide` at 0.93%/0.32%.  Both are ICF artefacts, not
  real call edges; the same samples carry garbage addresses
  (`0x8d4807894800154f`) one frame further up, which confirms the unwind is
  the failure, not the code.

## 1. Cluster attribution

`attr.py` walks `script.txt` and, for each leaf, reports the nearest
own-code ancestor frame.  Full table in `attr_all.txt`.

### Solid (frame-1 resolved and plausible)

| leaf | ancestor | share |
|---|---|---|
| `_copy_from_user` (564) | `Com_GetRealEvent` | 99.3% |
| `__recvmsg_syscall` (224) | `Com_GetRealEvent` | 100% |
| `entry_SYSRETQ_unsafe_stack` (329, atom) | `Com_GetRealEvent` | 95.7% |
| `___pthread_mutex_lock` (513) | `Com_GetRealEvent` 85.4%, `Com_Frame` 5.7% |
| `__strcasecmp_l_avx2` (387) | `Event::FindEvent` | 46.3% |

So the whole event-pump cluster -- poll, recvmsg, `copy_from_user`, and the
Xlib display lock -- funnels into `Com_GetRealEvent`
(`qcommon/common.c:1059` -> `Com_GetSystemEvent:1022` ->
`sdl/sdl_input.c:1696 Sys_SendKeyEvents` -> `IN_ProcessEvents:1436` ->
`SDL_PollEvent`).  The lock samples are Xlib's own `XLockDisplay` inside
`XCheckIfEvent`/`_XEventsQueued`, not a port lock.

Per frame the pump runs more than once: `common.c:2864`'s
`do { com_frameTime = Com_EventLoop(); msec = ... } while (msec < minMsec)`
re-pumps on every iteration until 1ms has passed, and `common.c:2968` pumps a
second time after `SV_Frame`.  That is 3-4 full X11 drains per frame, at 400
frames/s.

`SDL_JoystickUpdate` is NOT a contributor: `IN_JoyMove` (`sdl_input.c:817`)
returns before it when no stick is open, and game.log records no
joystick/gamepad init at all.

### Unreliable (ICF-polluted, recorded so nobody re-derives them)

`UIWidget::update`'s callers, `Menu::ForceHide`'s callers, `Cvar_FindVar`'s
callers and most of the CM trace callers resolved to nothing usable.  The
`unzReadCurrentFile` leaf (6.38% core, the largest single entry) resolved no
own-code ancestor at all.  Source enumeration, not the callchain, is what
settled the sections below.

## 2. The CM trace cluster: the prune hypothesis was TESTED AND REFUTED

The brief asked for caller attribution on `CM_TraceThroughBrush` (4.32%/3.51%)
and `CM_TraceToLeaf` (4.30%/3.56%).  7491 of 7529 CM-leaf samples resolved no
own-code caller (the unwinder stops at the leaf), so the algorithm itself is
what got examined.

`cm_trace.c:893` and `:1494` both read, for non-axial planes
(`plane->type >= 3`):

    if ( tw->isPoint ) { offset = 0; } else { offset = 2048; }

while the axial branch uses the tight `tw->extents[plane->type]`.  `2048`
makes a box trace descend BOTH children of effectively every non-axial node.
`traceWork_t::maxOffset` (`cm_local.h:176`) is exactly the correct bound
(`extents[0]+extents[1]+extents[2]`, what upstream Quake3 uses here) and it is
already computed per trace at `cm_trace.c:1017` and `:1597` -- and read
nowhere.  That looked like the whole cluster.

Both became `tw->maxOffset`, and a differential rig was built,
`cmtrace_probe.c`, which loads a real BSP's collision lumps into the engine's
own `clipMap_t` and runs each trace two ways: the shipped pruned traversal
from node 0, and `CM_TraceToLeaf` over EVERY leaf (no pruning at all),
comparing `trace_t` bit for bit.

Result with the fix in place, `m1l1a-borg_sphere.bsp`, six box sizes from
point to 96 units, seven masks, 20000 traces per box plus a sight-trace pass:

    RESULT PASS: 120000 traces compared, 0 mismatches
    sight: 120000 compared, 0 mismatches

Then the question was whether it bought anything, counting `c_brush_traces` over
the pruned leg only (`scratch/cmtrace_probe.c`, 5000 traces per box), before
vs after:

    box 1 (-15,-15,-24)-(15,15,32)   before 13044   after 13041
    box 2 (-15,-15,-18)-(15,15,32)   before 14319   after 14311
    box 3 (-12,-12,-4)-(12,12,4)     before 13143   after 13143
    box 4 (-32,-32,-16)-(32,32,16)   before 14356   after 14356
    box 5 (-48,-48,-48)-(48,48,48)   before 13925   after 13925

**Identical.**  The redundant descent the 2048 bound causes re-reaches leafs
whose brushes `checkcount` already deduplicated, so the brush tests -- which
are the two symbols the profile actually charges -- do not fall at all.
`CM_TraceThroughTree`'s own self time was only 0.53% in the profile, which is
the descent overhead this would have saved.  The cluster's cost is the NUMBER
of brush tests, i.e. the number of traces, not their pruning.

**Reverted** (`git checkout`); the two `2048` literals stand.  Landing a
change to gameplay collision code with no measured benefit would have been
risk without return.  The rig stays in `scratch/cmtrace_probe.c` for whoever
picks the cluster up: it is the tool for evaluating any future trace-count or
brush-test restructuring, and it already proves the plumbing.

Attribution note for the next attempt: server game logic runs at 10Hz
(`sv_main.c:666`, `frameMsec = 1000/sv_fps`), so the traces are cgame/client
at frame rate.  `CG_EntityShadow` (`cgame/cg_modelanim.c:639`) is one
`CM_BoxTrace` per shadow-casting entity per frame behind the
`RF_SHADOW` + distance + frustum gates at `:1133`.

## 3. The UI cluster: cvar resolution

The cluster's per-widget work is source-verifiable.  `UIWidget::update`
(`uilib/uiwidget.cpp:3123`) calls `updateVisibility`, `updateCvarVisibility`,
`updateStatVisibility` and `updateGameplayProperty`; all four gate on their
own flag and return immediately when unset, so the per-widget floor is four
cheap calls plus the children walk.  The real string-keyed work behind the
cluster:

* `testCvarTypes` (`uiwidget.cpp:2700`) does `uii.Cvar_Find` plus **two
  `atof` calls** per cvar per widget per frame, one of them re-parsing the
  widget's own constant `pair->value` every time.
* `GameplayDatabase::getObject` (`qcommon/gameplaydatabase.cpp`) walks a hash
  chain comparing `getFullScopeName() != objname`, and
  `GameplayObject::getProperty` (`:273`) is a linear scan over
  `_propertyList` comparing `str` -- recursing into `_baseObject`.
* `UIWindowManager::RegisterShader` (`uilib/uiwinman.cpp:950`) is a linear
  scan over `m_materiallist` comparing `str`.  Its callers in the grep are
  layout-time, not per-frame, so the 1.67%/0.63% is likely the same ICF
  pollution rather than a live hot path -- unproven either way.

### Landed: the cvar hash table

`Cvar_FindVar` (`qcommon/cvar.c:74`) walked a 256-bucket table;
`MAX_CVARS` (`:31`) caps the live population at 2048, so about eight cvars per
chain, each costing a `Q_stricmp`.  `FILE_HASH_SIZE` is now 4096.

Measured with the engine's own `Cvar_Get`/`Cvar_FindVar`
(`scratch/cvarbench.c`, 3M lookups):

| | 1200 cvars | 2048 cvars |
|---|---|---|
| hit | 41.3 -> **17.5 ns** | 62.6 -> **19.5 ns** |
| miss | 51.1 -> **33.5 ns** | 70.9 -> **36.3 ns** |

Why it is result-identical: two cvars with the same name hash to the same
bucket under any table size and keep insertion order within it, so the first
match `Cvar_FindVar` returns cannot change; and `hashIndex` (`q_shared.h:1133`)
is stored per cvar and only ever compared against this same table.
`scratch/cvarcorrect.c` checks the duplicate-name ordering, the
case-insensitive path, the miss path and a full table walk, and reads the same
on both table sizes.

**Prediction for the operator's next boot:** `Cvar_FindVar` drops from
0.85% (core) / 1.36% (atom) to roughly 0.3-0.5%, and the share of
`Q_stricmp`/`__strcasecmp_l_avx2` that Cvar_FindVar owns falls with it.  On
the controls page the per-frame saving is `(chainLen - 0.5) * Q_stricmp` per
cvar-bound widget per update.

### Considered and NOT landed, deliberately

A cached `cvar_t*` in `WidgetCvarPair` (which would remove the lookup
entirely) is unsafe: `CL_CleanupUserInfo` (`client/cl_main.cpp:634`) and
`Cvar_Restart` (`qcommon/cvar.c:1108`) both unset cvars out of the table, so a
pointer held across a map load dangles.  Increment A already removes most of
the chain walk, which was the charge.

## 4. The srf window instrument (r38's handover question)

`[PERF:shade]` names `srf=` -- the `RB_EndSurface`/`RB_BeginSurface` pair at
the shader change -- as the largest shade part at about 3us per stage draw,
and stops there.  Commit `61ca5b39` splits it into its own row:

    [PERF:srf] win=.. frames=.. surf/f=.. tot=.. endpre=.. stage=.. batche=..
               begin=.. oth=.. (usec per surface)

* `endpre` -- RB_EndSurface's guard-and-counter preamble
  (`tr_shade.c:1502` block), opened after the early-return gauntlets so
  nothing can be left open across an exit;
* `stage` -- the stage iterator call, net of the bind/record/pack already
  charged at their own sites (`vk_local.h:406`);
* `batche` -- `VK_Shade_BatchEnd` plus the showtris/normals tail;
* `begin` -- RB_BeginSurface's body, closed at each of its three exits;
* `tot` -- the window measured at its own seam (`VK_SrfPart_Disarm`), so
  `oth` = tot - the named four is a true residual and the five sum to `srf=`.

The window arms only across the pair (`tr_backend.c:1772`).  The sprite and
world walks reach the same two functions from five other call sites, so an
unguarded charge would have folded their traffic into this window's split; a
boot with the probe gate off pays one predictable test per sub-part and
nothing else.  The sub-parts are charged independently of the shade window's
own `VK_WS_*` parts, so `srf=`, `shade=` and `parts=` read exactly as before
-- this cannot perturb the accounting the operator already reads.

## 5. Not done, and why

* **A frame limiter.**  At 250-550 frames/s the whole per-frame cost is paid
  400 times a second, which is the single largest lever in the profile.  It is
  also a cvar the operator owns and a timing change, which is outside the
  pixel/gameplay bar.  Recorded, not acted on.
* **`Event::FindEvent` -> `strcasecmp` (46.3% of that symbol).**  Real, solid
  attribution, not attacked: it needs the same class of table analysis as the
  cvar fix and there was no budget left to do it and prove it.
* **`unzReadCurrentFile` (6.38% core, the largest single leaf).**  No own-code
  ancestor resolved.  It is `qcommon/files.cpp:1241`, i.e. every pak read, so
  it is probably load-time rather than per-frame -- unproven.
* **The lock cluster beyond the event pump.**  85.4% of `___pthread_mutex_lock`
  is Xlib inside `SDL_PollEvent`, which is the ACCEPTED-PLATFORM class; the
  in-class remainder is the 5.7% under `Com_Frame`, unattacked.

## Files

* `scratch/cmtrace_probe.c` -- the differential collision rig (BSP loader,
  pruned-vs-brute `trace_t` compare, sight-trace compare, bench mode)
* `scratch/probe_stubs.c` -- its link stubs
* `scratch/cvarbench.c`, `cvarbench_stubs.c` -- the cvar lookup benchmark
* `scratch/cvarcorrect.c` -- the cvar behaviour check
* `scratch/attr.py`, `attr_all.txt` -- the callchain attribution pass
* `script.txt` -- `perf script` dump of the operator's perf.data (37MB)
* `flat.txt` -- `perf report` flat symbol list, both event blocks
* `cm_chains.txt` -- the CM-leaf ancestor aggregation (the refutation input)
