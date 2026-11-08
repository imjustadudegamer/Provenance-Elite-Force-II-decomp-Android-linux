#!/usr/bin/env bash
# ProvenanceEF2 Linux release AppImage.
#
# Usage:   app/package_appimage.sh [--no-build] [--clean] [--jobs N] [--version X]
# Default: build the Linux target, then stage, verify and pack the AppImage.
#
# Output (app/appimage/dist/):
#   ProvenanceEF2-<version>-x86_64.AppImage
#   ProvenanceEF2-<version>-x86_64.AppImage.sha256
#   ProvenanceEF2-<version>-x86_64.build-info.txt
#
# The version comes from app/src/main/cpp/VERSION -- the same file cmake/
# identity.cmake reads for the engine banner and app/build.gradle.kts reads for
# the APK's versionName, so the AppImage, the engine banner and the APK cannot
# drift apart.
#
# The retail game data is not included and is never downloaded.  This pipeline
# never runs the game and never runs the produced AppImage; every check on the
# payload is static (ldd, readelf, objdump, desktop-file-validate,
# appstreamcli, --appimage-extract).  Runtime testing is the operator's.
#
# The Linux release is VULKAN ONLY (operator order): renderer_gles3.so is an
# Android-only concern and is configured out of this build with
# -DBUILD_RENDERER_GLES3=OFF.  The Android APK keeps both renderers and is
# untouched by this script.
#
# Conventions follow the same project's EF1 lane
# (VoyagerNX_Android/android-port-Single-Player/packaging/appimage): pinned
# tools, $ORIGIN-free AppDir driven by AppRun's LD_LIBRARY_PATH, a measured
# glibc floor, a build-info sidecar, and no execution anywhere.  Where the two
# trees differ the difference is stated in a comment.
set -euo pipefail

APP_NAME=ProvenanceEF2
APP_ID=io.github.imjustadudegamer.provenanceef2
ARCH=x86_64

PROJ="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$PROJ/.." && pwd)"
HERE="$PROJ/appimage"
CPP="$ROOT/app/src/main/cpp"
ENGINE_SRC="$CPP"
BUILD="$HERE/build"
ENGINE_BUILD="$BUILD/engine"
SNAP="$BUILD/snapshot"
APPDIR="$BUILD/appdir/$APP_NAME.AppDir"
LOGS="$BUILD/logs"
DIST="$HERE/dist"
# Tool downloads live OUTSIDE the repository (operator order): never committed,
# shared with any other AppImage lane on this machine.
TOOLS="${APPIMAGE_TOOLS_DIR:-$HOME/.local/share/appimage-tools}"

DO_BUILD=1
DO_CLEAN=0
JOBS="$(nproc 2>/dev/null || echo 4)"
VERSION_OVERRIDE=

usage() {
    sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
}

while [ $# -gt 0 ]; do
    case $1 in
        --no-build) DO_BUILD=0 ;;
        --clean)    DO_CLEAN=1 ;;
        --jobs)     JOBS=$2; shift ;;
        --version)  VERSION_OVERRIDE=$2; shift ;;
        -h|--help)  usage ;;
        *) printf 'unknown option: %s\n' "$1" >&2; usage ;;
    esac
    shift
done

log()  { printf '== %s\n' "$*"; }
info() { printf '   %s\n' "$*"; }
warn() { printf '!! %s\n' "$*" >&2; }
die()  { printf '!! error: %s\n' "$*" >&2; exit "${2:-1}"; }
sha256_of() { sha256sum "$1" | cut -d' ' -f1; }
ver_gt() {   # ver_gt A B -> true when A > B
    [ "$1" = "$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -1)" ] && [ "$1" != "$2" ]
}

# --------------------------------------------------------------------- version
# app/src/main/cpp/VERSION: one source of truth for the engine banner, the APK
# and this file name.  cmake/identity.cmake:21 reads it, app/build.gradle.kts:22
# reads it, and identity.cmake fails the configure if it is not MAJOR.MINOR.PATCH.
detect_version() {
    [ -n "$VERSION_OVERRIDE" ] && { printf '%s\n' "$VERSION_OVERRIDE"; return; }
    local v
    v=$(tr -d ' \t\n' < "$CPP/VERSION")
    [ -n "$v" ] || die "app/src/main/cpp/VERSION is empty"
    printf '%s\n' "$v"
}

VERSION=$(detect_version)
case $VERSION in
    *[!0-9.]*) die "app/src/main/cpp/VERSION must be MAJOR.MINOR.PATCH, got '$VERSION'" ;;
esac
APPIMAGE_NAME="$APP_NAME-$VERSION-$ARCH.AppImage"
APPIMAGE="$DIST/$APPIMAGE_NAME"

# Fixed timestamp for mksquashfs: the commit time, so re-packing the same tree
# gives the same image.  Overridable the usual way.
if [ -n "${SOURCE_DATE_EPOCH:-}" ]; then
    EPOCH=$SOURCE_DATE_EPOCH
elif git -C "$ROOT" rev-parse --git-dir >/dev/null 2>&1; then
    EPOCH=$(git -C "$ROOT" log -1 --format=%ct)
else
    EPOCH=0
fi
BUILD_DATE=$(date -u -d "@$EPOCH" +%Y-%m-%d 2>/dev/null || date -u +%Y-%m-%d)
GIT_SHA=$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo nogit)

mkdir -p "$LOGS" "$DIST" "$TOOLS"

if [ "$DO_CLEAN" -eq 1 ]; then
    log "cleaning"
    rm -rf "$ENGINE_BUILD" "$SNAP" "$APPDIR" "$BUILD/verify-extract"
    rm -f "$DIST"/*
    info "removed build/{engine,snapshot,appdir,verify-extract} and dist/*"
fi

# ----------------------------------------------------------------------- tools
# Pinned in app/appimage/tools.lock and verified on every run.  appimagetool is
# a *tagged* release, not `continuous`, and the runtime is supplied with
# --runtime-file so appimagetool never fetches "the latest runtime" behind our
# back -- the one non-reproducible step in a naive AppImage build.
# shellcheck source=/dev/null
. "$HERE/tools.lock"

verify_sha256() {
    local f=$1 want=$2 got
    [ -f "$f" ] || die "missing $f"
    got=$(sha256_of "$f")
    [ "$got" = "$want" ] || die "checksum mismatch on $f
  expected $want
  got      $got"
}

ensure_tools() {
    log "tools (pinned in app/appimage/tools.lock, cached in $TOOLS)"
    if [ ! -f "$TOOLS/appimagetool-x86_64.AppImage" ]; then
        info "downloading appimagetool $APPIMAGETOOL_VERSION"
        curl -fL --retry 3 -o "$TOOLS/appimagetool-x86_64.AppImage.part" "$APPIMAGETOOL_URL"
        mv "$TOOLS/appimagetool-x86_64.AppImage.part" "$TOOLS/appimagetool-x86_64.AppImage"
    fi
    verify_sha256 "$TOOLS/appimagetool-x86_64.AppImage" "$APPIMAGETOOL_SHA256"
    chmod +x "$TOOLS/appimagetool-x86_64.AppImage"
    info "appimagetool sha256 ok"

    if [ ! -f "$TOOLS/runtime-x86_64" ]; then
        info "downloading type-2 runtime $RUNTIME_RELEASE"
        curl -fL --retry 3 -o "$TOOLS/runtime-x86_64.part" "$RUNTIME_URL"
        mv "$TOOLS/runtime-x86_64.part" "$TOOLS/runtime-x86_64"
    fi
    verify_sha256 "$TOOLS/runtime-x86_64" "$RUNTIME_SHA256"
    info "runtime sha256 ok"

    # The runtime is GPG-signed.  A missing gpg is not fatal (the sha256 above is
    # the hard check); a *bad* signature is.
    if command -v gpg >/dev/null 2>&1; then
        [ -s "$TOOLS/signing-pubkey.asc" ] \
            || curl -fsSL --retry 3 -o "$TOOLS/signing-pubkey.asc" "$RUNTIME_PUBKEY_URL" || true
        [ -s "$TOOLS/runtime-x86_64.sig" ] \
            || curl -fsSL --retry 3 -o "$TOOLS/runtime-x86_64.sig" "$RUNTIME_SIG_URL" || true
        if [ -s "$TOOLS/signing-pubkey.asc" ] && [ -s "$TOOLS/runtime-x86_64.sig" ]; then
            local gh
            gh=$(mktemp -d)
            GNUPGHOME=$gh gpg --batch --import "$TOOLS/signing-pubkey.asc" >/dev/null 2>&1 || true
            if GNUPGHOME=$gh gpg --batch --verify "$TOOLS/runtime-x86_64.sig" \
                                             "$TOOLS/runtime-x86_64" >/dev/null 2>&1; then
                info "runtime signature ok ($RUNTIME_SIGNING_FPR)"
            else
                rm -rf "$gh"
                die "runtime GPG verification FAILED — refusing to package with it"
            fi
            rm -rf "$gh"
        else
            warn "runtime signature files unavailable; relying on the pinned sha256"
        fi
    fi
}

# ----------------------------------------------------------------------- build
# The linux-gles3 preset with the GLES 3.0 twin turned OFF.  BUILD_RENDERER_GLES3
# is the build system's own switch (app/src/main/cpp/CMakeLists.txt:113) and is
# legal with USE_RENDERER_DLOPEN on (cmake/renderer_common.cmake:42 only forbids
# it in a static build).  The client links no renderer at all when
# USE_RENDERER_DLOPEN is on (cmake/client.cmake:129-136), so dropping the twin
# does not change provenanceef2: it is byte-identical to the preset build.
find_toolchain() {
    # The pinned SDK cmake first, then whatever is on PATH -- the same order the
    # EF1 port's build-native.sh uses when it picks the cmake gradle pins.
    local d
    for d in "${ANDROID_SDK_ROOT:-$HOME/Android/sdk}/cmake/3.31.5/bin" \
             "${ANDROID_SDK_ROOT:-$HOME/Android/sdk}"/cmake/*/bin; do
        [ -x "$d/cmake" ] && { printf '%s\n' "$d"; return; }
    done
    printf '%s\n' ""
}

build_all() {
    local bindir
    bindir=$(find_toolchain)
    local cmake ninja_bin
    if [ -n "$bindir" ] && [ -x "$bindir/cmake" ]; then
        cmake="$bindir/cmake"
        ninja_bin="$bindir/ninja"
    else
        command -v cmake >/dev/null 2>&1 || die "no cmake found (install it, or set ANDROID_SDK_ROOT)"
        cmake=cmake
        ninja_bin=$(command -v ninja || true)
    fi
    [ -n "$ninja_bin" ] || die "no ninja found (install it, or install the SDK's cmake bundle)"
    info "cmake: $cmake"
    info "ninja: $ninja_bin"

    log "configuring the Linux target ($ENGINE_BUILD)"
    # EF2_PROBE_RELEASE_SILENT compiles every probe row family out of the
    # engine and the renderer, and restores logfile=0: a shipped Linux image
    # emits no row and writes no qconsole.log.  Fault paths (Sys_Error, the
    # signal handler, VK_Guard_Hit, crashlog.txt) never consult the gate, so
    # the crash channel is the one channel the image keeps.
    #
    # No CMAKE_*_RPATH is set.  The engine loads the renderer by path
    # (Sys_BinaryPath()/renderer_<x>.so) and the game modules by bare dlopen, so
    # AppRun's LD_LIBRARY_PATH covers every lookup; a $ORIGIN RUNPATH emitted by
    # CMake would carry a trailing empty element, which glibc reads as "also
    # search the current working directory".  Deliberately not shipped.
    "$cmake" -S "$ENGINE_SRC" -B "$ENGINE_BUILD" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DEF2_PLATFORM=linux \
        -DEF2_RENDERER=gles3 \
        -DEF2_DEFAULT_RENDERER=vulkan \
        -DBUILD_RENDERER_GLES3=OFF \
        -DEF2_PROBE_RELEASE_SILENT=ON \
        > "$LOGS/configure.log" 2>&1 \
        || { tail -30 "$LOGS/configure.log" >&2; die "cmake configure failed (logs/configure.log)"; }

    log "building provenanceef2 + renderer_vulkan.so + game.so + cgame.so"
    "$ninja_bin" -C "$ENGINE_BUILD" -j"$JOBS" \
        provenanceef2 renderer_vulkan.so game.so cgame.so version_refresh \
        > "$LOGS/build.log" 2>&1 \
        || { tail -40 "$LOGS/build.log" >&2; die "build failed (logs/build.log)"; }

    for f in provenanceef2 renderer_vulkan.so game.so cgame.so; do
        [ -f "$ENGINE_BUILD/Release/$f" ] || die "the build did not produce $f"
    done
    # Probes and the dedicated server are development output and must not ship.
    compgen -G "$ENGINE_BUILD/Release/*probe*" >/dev/null \
        && info "note: probe binaries are in the build tree; they are not staged"
    info "built: $(ls "$ENGINE_BUILD/Release" | tr '\n' ' ')"
}

# The tree is under active development, so a half-written .so must never reach
# the image: copy the outputs out and re-hash them.
snapshot_outputs() {
    log "snapshotting the build output"
    rm -rf "$SNAP"; mkdir -p "$SNAP"
    local f h1 h2
    for f in provenanceef2 renderer_vulkan.so game.so cgame.so; do
        h1=$(sha256_of "$ENGINE_BUILD/Release/$f")
        cp -L "$ENGINE_BUILD/Release/$f" "$SNAP/$f"
        chmod 0755 "$SNAP/$f"
        h2=$(sha256_of "$SNAP/$f")
        [ "$h1" = "$h2" ] || die "snapshot mismatch on $f — the build was written while it was read"
        info "  $f  $(stat -c%s "$SNAP/$f") bytes"
    done
}

# ------------------------------------------------------------------ AppDir stage
# Libraries that must come from the host, with the reason.  Everything else in
# the dependency closure is bundled.  Matching is on the SONAME (basename).
# The categories are the AppImage project's own excludelist
# (AppImage/pkg2appimage excludelist): glibc, the GL/Vulkan driver stack, the
# display server, the audio server, the C++ runtime, zlib.
#
# Deliberately NOT bundled:
#   * glibc and its parts — the loader is the host's, always.
#   * the GL/Vulkan *driver* stack — the loader reads the host's
#     /usr/share/vulkan/icd.d and loads the host driver.  The loader alone gets
#     a fallback copy in usr/lib/vulkan, never on LD_LIBRARY_PATH (see AppRun).
#   * libstdc++/libgcc_s — bundling them shadows the host's for everything that
#     dlopen()s into our process, and Mesa's Vulkan drivers are C++.
#   * X11 / Wayland / ALSA / PulseAudio — desktop-essential, configured by the
#     host, and their version must match the running display/audio server.
#   * zlib — ancient, every glibc system has it.
is_host_lib() {
    case $1 in
        # glibc
        libc.so.6|libm.so.6|libdl.so.2|libpthread.so.0|librt.so.1|libresolv.so.2|\
        libutil.so.1|libanl.so.1|libmvec.so.1|libthread_db.so.1|libBrokenLocale.so.1|\
        libnss_*.so.2|ld-linux-x86-64.so.2) return 0 ;;
        # GPU / display driver stack
        libGL.so.1|libEGL.so.1|libGLX.so.0|libGLdispatch.so.0|libOpenGL.so.0|\
        libgbm.so.1|libdrm.so.2|libglapi.so.0|libvulkan.so.1|libvulkan.so|\
        libvdpau.so.1|libva.so.2|libva-drm.so.2) return 0 ;;
        # X11 / Wayland / audio servers
        libX11.so.6|libX11-xcb.so.1|libxcb*.so*|libXext.so.6|libXau.so.6|libXdmcp.so.6|\
        libXcursor.so.1|libXi.so.6|libXfixes.so.3|libXrandr.so.2|libXss.so.1|libXrender.so.1|\
        libwayland-*.so*|libxkbcommon*.so*|libdecor-*.so*|\
        libasound.so.2|libpulse*.so*|libpulse-simple.so.0|libpipewire-*.so*|libjack.so.0|\
        libudev.so.1|libdbus-1.so.3|libsystemd.so.0|libcap.so.2) return 0 ;;
        # font rendering — the host's font stack matches the host's fonts
        libfontconfig.so.1|libfreetype.so.6|libharfbuzz*.so*|libfribidi.so.0) return 0 ;;
        # C++/gcc runtime — see the note above
        libstdc++.so.6|libgcc_s.so.1) return 0 ;;
        # base system
        libz.so.1|libz.so|libexpat.so.1|libuuid.so.1|liblzma.so.5|libzstd.so.1|\
        liblz4.so.1|libbsd.so.0|libmd.so.0|libgpg-error.so.0|libgcrypt.so.20|\
        libapparmor.so.1|libffi.so.8|libsndfile.so.1|libasyncns.so.0) return 0 ;;
    esac
    return 1
}

stage_libs() {
    # Breadth-first over the dependency closure: ldd each file, copy every
    # resolved library that is not on the host-only list, then ldd the copies.
    local queue=() copied=() skipped=()
    local f name path dest ldd_out
    queue=("$APPDIR/usr/bin/provenanceef2" \
           "$APPDIR/usr/bin/renderer_vulkan.so" \
           "$APPDIR/usr/bin/game.so" \
           "$APPDIR/usr/bin/cgame.so")

    while [ ${#queue[@]} -gt 0 ]; do
        f=${queue[0]}
        queue=("${queue[@]:1}")
        if ! ldd_out=$(ldd "$f" 2>/dev/null); then
            warn "ldd failed on $f"
            continue
        fi
        while read -r name path; do
            [ -n "$name" ] || continue
            if [ ${#copied[@]} -gt 0 ] && [[ " ${copied[*]} " == *" $name "* ]]; then continue; fi
            if is_host_lib "$name"; then
                skipped+=("$name")
                continue
            fi
            [ -e "$path" ] || { warn "$name resolved to $path which is not readable; not bundled"; continue; }
            dest=$APPDIR/usr/lib/$name
            cp -L "$path" "$dest"
            chmod 0644 "$dest"
            copied+=("$name")
            info "bundled $name  <- $path"
            queue+=("$dest")
        done < <(printf '%s\n' "$ldd_out" | awk '/=> \// {print $1, $3} /^\/.*\.so/ {print $1, $1}')
    done

    if [ -e "$APPDIR/usr/lib/libSDL2-2.0.so.0" ]; then
        # State which SDL got bundled.  The build host's is what the engine was
        # linked against, so pkg-config is the honest source for its version.
        local sdlv
        sdlv=$(pkg-config --modversion sdl2 2>/dev/null || true)
        info "SDL2 bundled: libSDL2-2.0.so.0${sdlv:+ (build host: $sdlv)}"
    else
        warn "libSDL2-2.0.so.0 was not found in the closure — the binary will need the host's SDL"
    fi
    printf '%s\n' "${skipped[@]-}" | sort -u > "$BUILD/host-libs.txt"
    info "host-provided: $(grep -c . "$BUILD/host-libs.txt" 2>/dev/null || echo 0) library name(s), see build/host-libs.txt"
}

stage_appdir() {
    log "staging AppDir"
    rm -rf "$APPDIR"
    mkdir -p "$APPDIR"/usr/bin "$APPDIR"/usr/lib \
             "$APPDIR"/usr/share/applications "$APPDIR"/usr/share/metainfo \
             "$APPDIR"/usr/share/icons/hicolor/256x256/apps \
             "$APPDIR"/usr/share/doc/provenanceef2 "$APPDIR"/usr/lib/vulkan

    # Our own four ELFs all go in usr/bin, beside the engine:
    #   provenanceef2        the engine
    #   renderer_vulkan.so   loaded by PATH -- Sys_LoadDll(name, qfalse) skips
    #                        the bare-name lookup and tries Sys_BinaryPath()
    #                        (code/client/cl_main.cpp:1884-1892,
    #                        code/sys/sys_main.c:601-611)
    #   game.so, cgame.so    loaded by bare dlopen (Sys_LoadGameDll,
    #                        code/sys/sys_main.c:689), resolved by AppRun's
    #                        LD_LIBRARY_PATH
    local n
    for n in provenanceef2 renderer_vulkan.so game.so cgame.so; do
        install -m 0755 "$SNAP/$n" "$APPDIR/usr/bin/$n"
    done

    # --- icon: the tree's own launcher asset, the only artwork it ships -------
    local icon="$ROOT/app/thirdparty/icon/ef2-launcher-256.png"
    [ -f "$icon" ] || die "missing app icon: $icon"
    install -m 0644 "$icon" "$APPDIR/$APP_ID.png"
    install -m 0644 "$icon" "$APPDIR/usr/share/icons/hicolor/256x256/apps/$APP_ID.png"
    ln -sf "$APP_ID.png" "$APPDIR/.DirIcon"

    # --- metadata -------------------------------------------------------------
    sed "s/__PROVENANCEEF2_VERSION__/$VERSION/g" "$HERE/AppRun" > "$APPDIR/AppRun"
    chmod 0755 "$APPDIR/AppRun"
    install -m 0644 "$HERE/$APP_ID.desktop" "$APPDIR/$APP_ID.desktop"
    install -m 0644 "$HERE/$APP_ID.desktop" "$APPDIR/usr/share/applications/$APP_ID.desktop"
    # appimagetool hardcodes the legacy file name (<desktop-id>.appdata.xml) when
    # it looks the component up, so that is the name used here; AppStream accepts
    # .appdata.xml and .metainfo.xml interchangeably.
    sed -e "s/__PROVENANCEEF2_VERSION__/$VERSION/g" \
        -e "s/__PROVENANCEEF2_DATE__/$BUILD_DATE/g" \
        "$HERE/$APP_ID.appdata.xml" > "$APPDIR/usr/share/metainfo/$APP_ID.appdata.xml"

    # --- licences -------------------------------------------------------------
    install -m 0644 "$ROOT/LICENSING.md" "$APPDIR/usr/share/doc/provenanceef2/LICENSING.md"
    [ -f "$ROOT/CREDITS.md" ] && install -m 0644 "$ROOT/CREDITS.md" \
        "$APPDIR/usr/share/doc/provenanceef2/CREDITS.md"
    [ -f "$ROOT/THIRDPARTY.md" ] && install -m 0644 "$ROOT/THIRDPARTY.md" \
        "$APPDIR/usr/share/doc/provenanceef2/THIRDPARTY.md"
    cat > "$APPDIR/usr/share/doc/provenanceef2/COPYING-NOTES.txt" <<'EOF'
ProvenanceEF2 is mixed-licence work: the engine lineage (id Tech 3 via ioquake3
and OpenMoHAA) is GPL-2.0-or-later, the Elite Force II SDK portions carry
Ritual's own limited-use licence, and the reconstructed client/UI code asserts
no copyright.  LICENSING.md in this directory is the authority; read it before
redistributing.

The bundled shared libraries are third-party components under their own
licences:
  SDL2        zlib licence (https://libsdl.org)

The Vulkan loader (usr/lib/vulkan, emergency fallback only) is Apache-2.0 or
Khronos MIT.

The retail game data (base/pak*.pk3 and the game DLL) is NOT part of this image
and is not redistributable: it must be supplied by the owner of the retail game.
EOF

    stage_libs

    # --- Vulkan loader fallback ----------------------------------------------
    # Neither the client nor renderer_vulkan.so links Vulkan (NEEDED: libm/libc/
    # libstdc++/libgcc_s/libSDL2 there), so the loader is a runtime requirement
    # that must never shadow the host's: it lives in usr/lib/vulkan, is not on
    # LD_LIBRARY_PATH, and AppRun only points SDL_VULKAN_LIBRARY at it when the
    # host has no loader at all.
    local loader
    # awk's exit can SIGPIPE ldconfig; under pipefail that would look like a
    # failure, so let it through and treat an empty result as "no loader".
    loader=$(ldconfig -p 2>/dev/null | awk '/libvulkan\.so\.1 /{print $NF; exit}') || true
    if [ -n "$loader" ] && [ -e "$loader" ]; then
        cp -L "$loader" "$APPDIR/usr/lib/vulkan/libvulkan.so.1"
        chmod 0644 "$APPDIR/usr/lib/vulkan/libvulkan.so.1"
        info "vulkan loader fallback: $loader"
    else
        rmdir "$APPDIR/usr/lib/vulkan" 2>/dev/null || true
        warn "no libvulkan.so.1 found to bundle as a fallback"
    fi

    log "stripping our binaries"
    for n in provenanceef2 renderer_vulkan.so game.so cgame.so; do
        strip --strip-unneeded "$APPDIR/usr/bin/$n"
    done

    info "$(du -sh "$APPDIR" | cut -f1) staged, $(find "$APPDIR" -type f | wc -l) files"
}

# ---------------------------------------------------------------- verification
# Paths (inside the AppDir) of every ELF this pipeline builds itself.
payload_elfs() {
    printf '%s\n' usr/bin/provenanceef2 usr/bin/renderer_vulkan.so \
                  usr/bin/game.so usr/bin/cgame.so
}

max_symbol_version() {   # $1 file, $2 prefix (GLIBC | GLIBCXX); empty when none
    objdump -T "$1" 2>/dev/null | grep -o "$2_[0-9.]*" | sort -V | tail -1 || true
}

verify_static() {
    log "static verification (nothing is executed)"
    local f g c r
    printf '   %-24s %-12s %-12s %s\n' file GLIBC GLIBCXX RUNPATH

    for f in $(payload_elfs); do
        g=$(max_symbol_version "$APPDIR/$f" GLIBC)
        c=$(max_symbol_version "$APPDIR/$f" GLIBCXX)
        r=$(readelf -d "$APPDIR/$f" 2>/dev/null \
            | awk '/RUNPATH|RPATH/{sub(/.*\[/,"");sub(/\].*/,"");printf "%s ", $0}')
        printf '   %-24s %-12s %-12s %s\n' "$(basename "$f")" "${g:-none}" "${c:-none}" "${r:-<none>}"
    done

    # The image's floor is the maximum over *everything* it carries, including
    # the bundled third-party libraries.  Measured, not assumed.
    local worst_glibc=0 worst_cxx=0 v elf
    while IFS= read -r elf; do
        v=$(max_symbol_version "$elf" GLIBC)
        if [ -n "$v" ]; then
            v=${v#GLIBC_}
            ver_gt "$v" "$worst_glibc" && worst_glibc=$v
        fi
        v=$(max_symbol_version "$elf" GLIBCXX)
        if [ -n "$v" ]; then
            v=${v#GLIBCXX_}
            ver_gt "$v" "$worst_cxx" && worst_cxx=$v
        fi
    done < <(find "$APPDIR/usr" -type f \( -name 'provenanceef2' -o -name '*.so' -o -name '*.so.*' \) | sort)

    GLIBC_FLOOR=$worst_glibc
    CXX_FLOOR=$worst_cxx
    printf '   %-24s %s\n' "max GLIBC required" "GLIBC_$GLIBC_FLOOR"
    printf '   %-24s %s\n' "max GLIBCXX required" "GLIBCXX_$CXX_FLOOR"
    # 2.35 is the Ubuntu 22.04 LTS floor the EF1 lane treats as its baseline.
    # This host is newer, so a host build links newer symbols; the warning states
    # the measured number rather than assuming a floor.
    if ver_gt "$GLIBC_FLOOR" "2.35"; then
        warn "the glibc floor is GLIBC_$GLIBC_FLOOR — this host is newer than Ubuntu 22.04,"
        warn "so the image will not start there.  The GitHub Actions lane (which builds on"
        warn "ubuntu-22.04, glibc 2.35) produces the wider-compatibility build."
    fi

    # No RUNPATH is set on purpose (see build_all); AppRun's LD_LIBRARY_PATH is
    # the mechanism, and it is on every path an AppImage can be started by.
    for f in $(payload_elfs); do
        if readelf -d "$APPDIR/$f" 2>/dev/null | grep -q 'RUNPATH\|RPATH'; then
            info "RUNPATH present in $(basename "$f") (harmless; AppRun's LD_LIBRARY_PATH still applies)"
        fi
    done

    # The release is probe-silent by construction (EF2_PROBE_RELEASE_SILENT
    # above).  Prove it on the payload instead of trusting the configure: these
    # row formats are compiled out by that macro and are present in a probe
    # build, so a stray flag or a hand-built .so fails here.
    local probe_rows=0
    for n in provenanceef2 renderer_vulkan.so; do
        # fingerprints chosen because PROBE_BUILD_SILENT removes them, not just
        # because their emitters are runtime-gated
        if strings -a "$APPDIR/usr/bin/$n" | grep -qE '^\[(PROBE:limit|PERF:frame|SMP:frame|VK:stencil|2D:sum)\]'; then
            probe_rows=$((probe_rows + 1))
            warn "$(basename "$n") carries probe row formats — the release is NOT probe-silent"
        fi
    done
    [ "$probe_rows" = "0" ] || die "probe rows are compiled into the Linux release payload"

    # No host build path may leak into the shipped binaries.  Checked through
    # `strings`, because the host's grep is aliased to ugrep here and its
    # binary-file count is not dependable; `strings` is what the rule is about
    # anyway (a path that only a strings dump can see still ships).
    local leaks=0 n
    for n in $(payload_elfs); do
        # grep without -q: -q makes grep exit on the first match, which SIGPIPEs
        # the upstream strings and -- under pipefail -- turns a *found* path into
        # a failed pipeline and a silently skipped check.
        if strings -a "$APPDIR/$n" | grep -F "$HOME" >/dev/null; then
            leaks=$((leaks + 1))
            warn "$(basename "$n") carries a build path:"
            strings -a "$APPDIR/$n" | grep -F "$HOME" | head -3 | sed 's/^/       /'
        fi
    done
    [ "$leaks" = "0" ] || warn "$leaks payload file(s) carry a build path — see \
cmake/renderer_common.cmake:59-61, which applies -ffile-prefix-map to the \
renderer but has no client equivalent"

    # Closure completeness: every NEEDED of every staged ELF must resolve from
    # the staged tree plus the host-provided set.
    local notfound
    notfound=$(env LD_LIBRARY_PATH="$APPDIR/usr/lib:$APPDIR/usr/bin" sh -c \
        "for f in '$APPDIR/usr/bin/provenanceef2' '$APPDIR/usr/bin/'*.so; do
             ldd \"\$f\" 2>/dev/null | awk '/not found/{print \$1}'
         done | sort -u") || true
    if [ -n "$notfound" ]; then
        warn "libraries not resolvable in the staging environment:"
        printf '     %s\n' $notfound >&2
    else
        info "closure: every NEEDED library resolves"
    fi

    # ldd -r: undefined symbols must be resolvable from the staged tree.  The
    # game modules reach the engine through the game_import_t / clientImport_t
    # tables, not through dynamic symbols, so they are expected to be clean too.
    local undefined
    for f in $(payload_elfs); do
        undefined=$(env LD_LIBRARY_PATH="$APPDIR/usr/lib:$APPDIR/usr/bin" sh -c \
            "ldd -r '$APPDIR/$f' 2>&1 | grep -c 'undefined symbol'" || true)
        [ "${undefined:-0}" = "0" ] \
            || warn "ldd -r $(basename "$f"): $undefined undefined symbol(s)"
        info "ldd -r $(basename "$f"): ${undefined:-0} undefined symbol(s)"
    done

    # ---- the Linux release is VULKAN ONLY -----------------------------------
    [ -f "$APPDIR/usr/bin/renderer_vulkan.so" ] \
        || die "renderer_vulkan.so is not in the payload — refusing to ship an image that cannot start"
    if compgen -G "$APPDIR/usr/bin/renderer_gles3*.so" >/dev/null; then
        die "renderer_gles3*.so is in the payload — the Linux release is Vulkan only"
    fi
    compgen -G "$APPDIR/usr/bin/*probe*" >/dev/null && \
        die "probe binaries reached the payload"
    [ -f "$APPDIR/usr/bin/provenanceef2ded" ] \
        && die "the dedicated server reached the payload"
    info "payload: engine + renderer_vulkan.so + game.so + cgame.so (Vulkan only, no probes, no server)"

    # ---- the version stamp inside the binary must match the file name -------
    # PRODUCT_VERSION_FULL is "<VERSION>+<short sha><-dirty>" (code/qcommon/
    # q_version.h:65), compiled in at build time from the same VERSION file the
    # file name came from.
    local stamp
    stamp=$(strings -a "$APPDIR/usr/bin/provenanceef2" \
            | grep -E "^${VERSION}\+[0-9a-f]{6,40}(-dirty)?$" | head -1 || true)
    [ -n "$stamp" ] || die "the packaged binary does not carry the ${VERSION}+<sha> stamp — \
it is not built from this tree's version"
    info "binary version stamp: $stamp (file name: $APPIMAGE_NAME)"

    # AppRun must be a valid POSIX shell script and executable.
    sh -n "$APPDIR/AppRun" || die "AppRun has a syntax error"
    [ -x "$APPDIR/AppRun" ] || die "AppRun is not executable"

    command -v desktop-file-validate >/dev/null 2>&1 \
        && { desktop-file-validate "$APPDIR/$APP_ID.desktop" && info "desktop file: valid"; }
    if command -v appstreamcli >/dev/null 2>&1; then
        # validate-tree is exactly what appimagetool runs on the AppDir (desktop
        # file + metadata + icon together), so catching it here fails early.
        if appstreamcli validate-tree "$APPDIR" > "$LOGS/appstream.log" 2>&1; then
            info "AppStream metadata: valid (appstreamcli validate-tree)"
        else
            cat "$LOGS/appstream.log" >&2
            die "appstreamcli validate-tree failed — appimagetool would refuse this AppDir"
        fi
    fi
    return 0
}

# --------------------------------------------------------------------- package
make_appimage() {
    log "appimagetool $APPIMAGETOOL_VERSION (zstd, pinned runtime)"
    # APPIMAGE_EXTRACT_AND_RUN: appimagetool is itself an AppImage; this lets it
    # run without FUSE (containers, CI).  It does not run our payload.
    ARCH=$ARCH VERSION="$VERSION" APPIMAGE_EXTRACT_AND_RUN=1 \
    "$TOOLS/appimagetool-x86_64.AppImage" \
        --runtime-file "$TOOLS/runtime-x86_64" \
        --comp zstd \
        --mksquashfs-opt -Xcompression-level --mksquashfs-opt 19 \
        --mksquashfs-opt -all-time --mksquashfs-opt "$EPOCH" \
        "$APPDIR" "$APPIMAGE" > "$LOGS/appimagetool.log" 2>&1 \
        || { tail -30 "$LOGS/appimagetool.log" >&2; die "appimagetool failed (logs/appimagetool.log)"; }
    grep -iE 'warn|appstream' "$LOGS/appimagetool.log" | sed 's/^/   /' || true
    info "$APPIMAGE_NAME: $(stat -c%s "$APPIMAGE") bytes"
}

verify_appimage() {
    log "verifying the image without running it"
    # --appimage-extract executes only the runtime's extraction path: AppRun
    # (the game) is not executed by this pipeline.
    local tmp="$BUILD/verify-extract" diff_out
    rm -rf "$tmp"; mkdir -p "$tmp"
    ( cd "$tmp" && "$APPIMAGE" --appimage-extract > "$LOGS/appimage-extract.log" 2>&1 ) \
        || { tail -20 "$LOGS/appimage-extract.log" >&2; die "the AppImage could not be extracted (broken image)"; }

    if diff_out=$(diff -r "$APPDIR" "$tmp/squashfs-root" 2>&1); then
        info "payload round-trip: squashfs-root is identical to the staged AppDir"
    else
        printf '%s\n' "$diff_out" | head -20 >&2
        die "the extracted payload differs from the staged AppDir"
    fi
    rm -rf "$tmp"

    # type-2 magic + squashfs superblock, and the runtime must be the pinned one.
    python3 - "$APPIMAGE" "$(stat -c%s "$TOOLS/runtime-x86_64")" <<'PY'
import sys
path, rt_len = sys.argv[1], int(sys.argv[2])
d = open(path, 'rb').read(1 << 20)
assert d[:4] == b'\x7fELF', 'not an ELF file'
assert d[8:11] == b'AI\x02', 'not an AppImage type 2'
# "hsqs" also occurs inside the runtime's own code, so look past the runtime.
i = d.find(b'hsqs')
while i != -1 and i < rt_len:
    i = d.find(b'hsqs', i + 1)
assert i > 0, 'no squashfs superblock after the runtime'
print(f'   AppImage type 2: runtime {rt_len} bytes, squashfs superblock at offset {i}')
PY
    # The embedded runtime must be the pinned one.  appimagetool writes the
    # payload's MD5 into the runtime's tail, so byte identity does not hold; the
    # ELF build ID is the identity of the runtime binary itself.
    local rid pid
    rid=$(readelf -n "$APPIMAGE" 2>/dev/null | awk '/Build ID/{print $3; exit}')
    pid=$(readelf -n "$TOOLS/runtime-x86_64" 2>/dev/null | awk '/Build ID/{print $3; exit}')
    if [ -n "$rid" ] && [ "$rid" = "$pid" ]; then
        info "embedded runtime: BuildID $rid = pinned $RUNTIME_RELEASE runtime"
    else
        die "the embedded runtime is not the pinned one (build id '$rid' vs '$pid')"
    fi
}

write_release_info() {
    log "recording the build"
    {
        printf 'ProvenanceEF2 %s — AppImage build information\n' "$VERSION"
        printf 'packaged %s (SOURCE_DATE_EPOCH=%s)\n\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" "$EPOCH"
        printf 'builder         : host (%s)\n' "$( . /etc/os-release; echo "$PRETTY_NAME" )"
        printf 'git             : %s\n' "$GIT_SHA"
        printf 'glibc floor     : GLIBC_%s (max symbol version required)\n' "$GLIBC_FLOOR"
        printf 'libstdc++ floor : GLIBCXX_%s\n' "$CXX_FLOOR"
        printf 'appimagetool    : %s  sha256 %s\n' "$APPIMAGETOOL_VERSION" "$APPIMAGETOOL_SHA256"
        printf 'runtime         : %s  sha256 %s  (GPG %s)\n' "$RUNTIME_RELEASE" "$RUNTIME_SHA256" "$RUNTIME_SIGNING_FPR"
        printf 'SDL2            : the copy the build linked (bundled)\n'
        printf 'squashfs        : zstd level 19, all-time %s\n' "$EPOCH"
        printf 'probes          : compiled out (EF2_PROBE_RELEASE_SILENT=ON); the crash\n'
        printf '                  backtrace, crashlog.txt and the fault rows stay ungated\n\n'
        printf 'payload (built from the tree in this run):\n'
        for f in provenanceef2 renderer_vulkan.so game.so cgame.so; do
            printf '  %-20s %10s  sha256 %s\n' "$f" "$(stat -c%s "$SNAP/$f")" "$(sha256_of "$SNAP/$f")"
        done
        printf '\nrenderer         : Vulkan only (BUILD_RENDERER_GLES3=OFF).  The GLES 3.2\n'
        printf '                   twin is the Android APK concern and is not packaged here.\n'
        printf '\nmodule placement: all four beside the engine in usr/bin.\n'
        printf '  renderer_vulkan.so  by PATH: Sys_LoadDll(name, qfalse) skips the bare-name\n'
        printf '                      lookup and tries Sys_BinaryPath() first\n'
        printf '                      (cl_main.cpp:1884, sys_main.c:601).\n'
        printf '  game.so / cgame.so  by bare dlopen (Sys_LoadGameDll, sys_main.c:689),\n'
        printf '                      resolved by AppRun'"'"'s LD_LIBRARY_PATH.\n'
        printf 'No RUNPATH is emitted: CMake would write a trailing empty element, which\n'
        printf 'glibc reads as "also search the current working directory".\n'
        printf '\nbundled libraries (%s):\n' "$(ls "$APPDIR/usr/lib" | grep -c .)"
        ls "$APPDIR/usr/lib" | grep -v '^vulkan$' | sed 's/^/  /'
        printf '  vulkan/libvulkan.so.1   (fallback only, not on LD_LIBRARY_PATH)\n'
        printf '\nleft to the host (%s, direct dependencies only):\n' "$(grep -c . "$BUILD/host-libs.txt" || true)"
        sed 's/^/  /' "$BUILD/host-libs.txt"
        printf '\nhost must also provide (loaded dynamically, not in ldd):\n'
        printf '  %s\n' \
            "glibc >= $GLIBC_FLOOR and libstdc++ >= GLIBCXX_$CXX_FLOOR" \
            'libvulkan.so.1 + a working Vulkan ICD/driver for the GPU' \
            'X11 client libraries (libX11, libXcursor, libXrandr, libXi, libXfixes, libXss) or XWayland' \
            'libasound2 and libpulse0 for sound (SDL needs both, and loads them at run time)'
        printf '\nthe AppImage was never executed by this pipeline; runtime testing is the operator'"'"'s job\n'
    } > "$DIST/$APP_NAME-$VERSION-$ARCH.build-info.txt"
    sha256sum "$APPIMAGE" | sed "s|$DIST/||" > "$APPIMAGE.sha256"
    info "dist/$APPIMAGE_NAME"
    info "dist/$APPIMAGE_NAME.sha256"
    info "dist/$APP_NAME-$VERSION-$ARCH.build-info.txt"
}

# ------------------------------------------------------------------------ main
main() {
    log "$APP_NAME $VERSION"
    ensure_tools
    if [ "$DO_BUILD" -eq 1 ]; then
        build_all
    else
        info "--no-build: using the existing $ENGINE_BUILD/Release"
        [ -f "$ENGINE_BUILD/Release/provenanceef2" ] \
            || die "no build to package at $ENGINE_BUILD/Release (run without --no-build)"
        [ -f "$ENGINE_BUILD/Release/renderer_vulkan.so" ] \
            || die "no renderer_vulkan.so in $ENGINE_BUILD/Release — the Linux release is Vulkan only"
        [ -f "$ENGINE_BUILD/Release/game.so" ] || die "no game.so in $ENGINE_BUILD/Release"
        [ -f "$ENGINE_BUILD/Release/cgame.so" ] || die "no cgame.so in $ENGINE_BUILD/Release"
    fi
    snapshot_outputs
    stage_appdir
    verify_static
    make_appimage
    verify_appimage
    write_release_info

    log "done"
    printf '   %-14s %s\n' "AppImage" "$APPIMAGE"
    printf '   %-14s %s\n' "size" "$(du -h "$APPIMAGE" | cut -f1)"
    printf '   %-14s %s\n' "sha256" "$(sha256_of "$APPIMAGE")"
    printf '   %-14s GLIBC_%s\n' "glibc floor" "$GLIBC_FLOOR"
    echo
    echo "   The image was NOT executed: this pipeline never launches the game."
    echo "   Runtime testing is the operator's job."
}

main
