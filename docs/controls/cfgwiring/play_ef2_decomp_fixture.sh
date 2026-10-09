#!/bin/bash
# play_ef2_decomp_fixture.sh -- the launcher fragment this control audits when
# the real script is not in the tree.  Regenerate from
# personal/scripts/play_ef2_decomp.sh whenever its settings block, its writer
# or its engine command line change; the control prefers the live script.
RENDERER_CVAR="vulkan"
BASE="${EF2_RUN_DIR:-$HOME/.cache/ef2-android/airun}/base"

LAUNCHER_OVERRIDES=(
    "cl_renderer=$RENDERER_CVAR"
    "r_fullscreen=0"  "r_mode=-1"  "r_customwidth=800"  "r_customheight=600"
    "s_initsound=1"  "sv_pure=0"
    "developer=0"  "ui_console=0"  "com_maxfps=0"  "logfile=0"
    "r_vkSafeMode=0"  "r_smp=1"  "r_lightmap=0"  "r_worldFlat=0"
    "r_vertexLight=0"  "r_vboStatic=0"
    "vk_2dCover=2"
    "r_jobs=-1"  "cl_jobs=-1"  "r_swapInterval=0"
)
if [ "$DIAG" -eq 1 ]; then
    LAUNCHER_OVERRIDES+=( "qa_probe=1" "r_footprintProbe=1" "r_vkValidate=1"
                          "ui_statsOn=1" "d2_stallStats=1" "r_perfFps=1"
                          "smp_loadStats=1"
                          "logfile=3" )
    echo "mode: DIAGNOSTIC (--diag) -- validation + all probe families + engine log + host monitors ON" >&2
else
    LAUNCHER_OVERRIDES+=( "qa_probe=0" "r_vkValidate=0" "r_footprintProbe=0"
                          "ui_statsOn=0" "d2_stallStats=0" "r_perfFps=0"
                          "smp_loadStats=0" )
    echo "mode: QUIET -- validation, probes, engine log and host monitors OFF; --diag arms them" >&2
    echo "        a QUIET run dir holds game.log (the crash channel) and nothing else" >&2
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
