#!/bin/bash
# play_ef2_decomp_fixture.sh -- the launcher fragment this control audits when
# the real script is not in the tree.  Regenerate from
# personal/scripts/play_ef2_decomp.sh whenever its settings block, its writer
# or its engine command line change; the control prefers the live script.
RENDERER_CVAR="vulkan"
BASE="${EF2_RUN_DIR:-$HOME/.cache/ef2-android/airun}/base"
LAUNCHER_OVERRIDES=(
    # renderer: this launch's .so, never the config's stale one
    "cl_renderer=$RENDERER_CVAR"
    # window: the run's own 800x600, not the config's mode
    "r_fullscreen=0"  "r_mode=-1"  "r_customwidth=800"  "r_customheight=600"
    # engine
    "s_initsound=1"  "sv_pure=0"
    "developer=0"  "ui_console=0"  "com_maxfps=0"  "logfile=3"
    # the A/B block (operator orders 2026-10-15)
    "r_vkSafeMode=1"  "r_smp=0"  "r_lightmap=1"
    # worker pool auto-sizing, and no vsync cap
    "r_jobs=-1"  "cl_jobs=-1"  "r_swapInterval=0"
)
# pix_probe is deliberately NOT set by this launcher: it answers to the shared
# probe gate, and forcing it 0 suppresses the other instrument families (PROGCACHE
# / imgup / UI rows).  Set it per-boot only for a specific experiment.
if [ "$PERF_ONLY" -eq 1 ]; then
    LAUNCHER_OVERRIDES+=( "qa_probe=0" "r_vkValidate=0" "r_footprintProbe=0"
                          "ui_statsOn=0" "d2_stallStats=0" "r_perfFps=1" )
    echo "mode: CLEAN PERFORMANCE (--perf) -- validation, probe rows and census OFF" >&2
else
    LAUNCHER_OVERRIDES+=( "qa_probe=1" "r_footprintProbe=1" "r_vkValidate=1"
                          "ui_statsOn=1" "d2_stallStats=1" "r_perfFps=1" )
    echo "mode: DIAGNOSTIC (validation + all probe families ON; use --perf for a clean read)" >&2
fi

OVERRIDE_FILE="$BASE/ef2-launcher.cfg"
: > "$OVERRIDE_FILE" || { echo "cannot write $OVERRIDE_FILE" >&2; exit 1; }
for kv in "${LAUNCHER_OVERRIDES[@]}"; do
    k="${kv%%=*}"; v="${kv#*=}"
    case "$v" in
        ''|*[[:space:]]*) printf 'setx %s "%s"\n' "$k" "$v" >> "$OVERRIDE_FILE" ;;
        *)                printf 'setx %s %s\n'  "$k" "$v" >> "$OVERRIDE_FILE" ;;
    esac
done
echo "overrides: $OVERRIDE_FILE (${#LAUNCHER_OVERRIDES[@]} settings, setx: win over config.cfg, never archived)"

${EF2_WRAPPER:-} ./provenanceef2 \
    +set fs_basepath "$FS" +set fs_homepath "$FS" \
    +set com_execAfterConfig ef2-launcher.cfg \
    "${ENGINE_ARGS[@]}" > "$LOGFILE" 2>&1 &
PIPE=$!
