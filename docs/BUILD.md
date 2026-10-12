# Build

One source tree, two targets: Android (arm64-v8a, the shipping Vulkan backend,
with the GLES 3.0 twin built beside it) and Linux x86-64 (the same engine, used
for the offline corpora, the desktop reference and local play). There is no fork:
`EF2_PLATFORM` selects the platform glue, `EF2_RENDERER=gles3` gates the GLES 3.0
twin, and `EF2_DEFAULT_RENDERER=vulkan` is `cl_renderer`'s compiled default.

## Android

Prerequisites: Android SDK (`android-35`), NDK `27.0.12077973`, CMake `3.31.5`,
JDK 17+, Gradle 8.13 (the wrapper pins it). Point Gradle at the SDK with a
gitignored `local.properties`:

```sh
echo "sdk.dir=$HOME/Android/sdk" > local.properties
app/build_apk.sh assembleDebug          # -> app/build/outputs/apk/debug/app-debug.apk
app/build_apk.sh assembleRelease        # the release variant, same signing key
```

`build_apk.sh` runs the tree-isolation guard first (a build must not absorb
untracked or modified work the CMake globs would compile in), then Gradle's
`externalNativeBuild`, which configures the engine CMake with the NDK toolchain
and `-DEF2_PLATFORM=android -DEF2_RENDERER=gles3`. `EF2_DEFAULT_RENDERER` is not
passed here: it keeps its own CMake default of `vulkan`, which is what
`cl_renderer` compiles to.
When `$HOME` is not writable
it redirects Gradle and ccache state to copies under `personal/evidence/` and prints
the substitution; a normal interactive shell is unaffected.

## Linux reference

```sh
cd app/src/main/cpp
cmake --preset linux-gles3
ninja -C build -j16
```

Binaries under `build/Release/`:

| file | what it is |
|---|---|
| `provenanceef2` | the client engine (client + server + uilib + botlib + tiki/skeletor) |
| `provenanceef2ded` | the dedicated server |
| `game.so` | the game module, dlopened at run time |
| `cgame.so` | the client-game module, dlopened at run time |
| `renderer_vulkan.so` | the shipping Vulkan backend, `cl_renderer`'s default |
| `renderer_gles3.so` | the GLES 3.0 reference twin |

The probe binaries in the same directory (`gles3_firstlight`,
`smp_hwtex_probe`, `progcache_probe`, …) are offline evidence harnesses for the
GLES 3.0 twin. They are built by the Linux configuration and are never executed.

## The gate

Every change must pass, on a clean rebuild:

- `ninja -j16`, 0 warnings / 0 errors;
- `ldd -r`, 0 undefined on all six binaries;
- `tools/license_lint.py`, `tools/ship_text_lint.py`,
  `tools/comment_budget.py` and `tools/provenance_ledger.py --check`, all green.

## What ships in the APK

| file | what it is |
|---|---|
| `libmain.so` | the whole engine client; `SDL_main` is its entry point |
| `libgame.so`, `libcgame.so` | the game / client-game modules, dlopened as on the desktop |
| `librenderer_vulkan.so` | the shipping Vulkan backend, `cl_renderer`'s default |
| `librenderer_gles3.so` | the GLES 3.0 reference twin |
| `libSDL2.so`, `libopenal.so`, `libc++_shared.so` | window/event/context, OpenAL Soft, the NDK runtime |
| `assets/base/zpak-android.pk3` | the on-screen touch art, the front-end text override, and `global/{default,defaultcontrols,defaultoptions}.cfg` (the boot baseline plus the Restore Controls / Restore Defaults payloads) |

Retail game data is not distributed. `zpak-android.pk3` is generated from
`code/qcommon/ef2_android_defaults.h` by `app/make_zpak.py` (`python3
app/make_zpak.py`, with `--check` verifying it is current), so the binary
defaults and the pak cannot drift; it is the only pack the build ever
regenerates. `Assets/base/` changes require a `versionCode` bump
(`10500` in `app/build.gradle.kts`), or an existing install keeps its extracted
copy.

The manifest's `versionName` comes from the single version source,
`app/src/main/cpp/VERSION`; Gradle appends the build's commit so two installs
are distinguishable in a manifest dump. The manifest declares no permissions.

## Desktop launcher

The untracked `personal/scripts/play_ef2_decomp.sh` is the Linux boot path. It
owns the per-boot config override: a fresh `ef2-launcher.cfg` is written and
handed to the engine through `com_execAfterConfig` with `setx` values, so the
launcher's overrides win over the `config.cfg` the engine execs, on every boot,
regardless of what that config carried last. `--legacy-audio` flips the mixer
off. Quiet is the default on every target: `--diag` is the only arm (validation,
probe families, engine log, host monitors), and `--quiet` is kept as the explicit
no-op.

## Run-time layout

The engine's `fs_basepath` is the app's OBB directory,
`/storage/emulated/0/Android/obb/com.provenanceef2`, where the retail `base/` is
imported. The write root (`fs_homepath` and the `fs_home*` paths) is
`<obb>/provenanceef2`, created on first run; saves, config, demos and screenshots
live there and are pulled with `adb pull <obb>/provenanceef2/`. The two are kept
apart because a new subdirectory cannot be created by the app in the pushed OBB
tree; if the device refuses one, the engine falls back to the app's internal
files directory and prints the chosen root once per launch.
