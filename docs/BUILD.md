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
and `-DEF2_PLATFORM=android -DEF2_RENDERER=gles3 -DEF2_DEFAULT_RENDERER=vulkan`.
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
| `assets/base/zpak-android.pk3` | the on-screen touch art plus `global/default.cfg`, `global/defaultcontrols.cfg`, `global/defaultoptions.cfg` |

Retail game data is not distributed. `zpak-android.pk3` is generated from
`code/qcommon/ef2_android_defaults.h` by `app/make_zpak.py`, so the binary
defaults and the pak cannot drift. `Assets/base/` changes require a
`versionCode` bump, or an existing install keeps its extracted copy.

## Run-time layout

The engine's `fs_basepath` is the app's OBB directory,
`/storage/emulated/0/Android/obb/com.provenanceef2`, where the retail `base/` is
imported. The write root (`fs_homepath` and the `fs_home*` paths) is
`<obb>/provenanceef2`, created on first run; saves, config, demos and screenshots
live there and are pulled with `adb pull <obb>/provenanceef2/`. The two are kept
apart because a new subdirectory cannot be created by the app in the pushed OBB
tree; if the device refuses one, the engine falls back to the app's internal
files directory and prints the chosen root once per launch.
