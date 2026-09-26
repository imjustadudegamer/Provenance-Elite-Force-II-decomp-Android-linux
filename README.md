# Provenance: Elite Force II decomp (Android/linux)

A native port of *Star Trek: Voyager: Elite Force II* (Ritual Entertainment,
published by Activision, 2003) to Android and Linux. The retail game runs on a
de-MOHAAd id Tech 3 engine, the [OpenMoHAA](https://github.com/openmoh/openmohaa)
lineage that descends from id Software's Quake III Arena GPL release, with a
Vulkan renderer written for this port and the GLES 3.0 backend built beside it
as the reference twin. The engine, the game, the client game,
the renderer and the model layer all build from one tree in `app/src/main/cpp/`.

**Download the APK from Releases and install it. You do not need to build
anything to play.** Releases carry the Android APK only. There is no prebuilt
Linux binary, so Linux builds from source. See [Linux](#linux).

**No game data is included.** You need your own copy of *Star Trek: Voyager:
Elite Force II*. On first launch the app asks you to select your retail files,
the game's `.pk3` paks and the game DLL, and copies them into place.

## Why "Provenance"?

Every function, struct, table and constant in this port is held to a witness.
Where the original source was published, that source is the authority: id
Software's GPL engine release, and Ritual's own Elite Force II fgame code.
Everything else is read out of the shipping retail binaries, the little known Mac
PPC release first and the PC build as a secondary corroboration, and every value
is checked against the retail data that ships with the game. A claim that cannot
be tied to one of those is recorded as open rather than asserted. The name says
that the code carries its own evidence chain. Ask where a behaviour comes from
and there is an answer.

## Features

- **Vulkan renderer**, written for this port and the one it ships with. The
  GLES 3.0 backend is built beside it as the reference twin and selected with
  `cl_renderer gles3`.
- **Touch controls**: a virtual stick, a look area, and on-screen pills for fire,
  jump, crouch, use and reload, with the on-screen art packed into the app.
- **Full gamepad support** through SDL's GameController API. Dual sticks,
  triggers, face buttons and D-pad, sharing one key map with the touch controls,
  so rebinding either moves both.
- **First-run game import** through the system file picker. Pick your paks and
  the game DLL once and they are copied into place. No storage permission is
  needed at any target SDK.
- **Widescreen support**, with aspect handling for the 3D view, the HUD and the
  2D menus.
- **One tree, two targets.** Android and Linux x86-64 build from the same sources
  behind a platform switch.
- **16 KB page-size alignment** for Android 15+, with a 16 KB-aligned C++ runtime.

## Requirements

- **Android 8.0 (API 26) or newer**, `arm64-v8a`, a Vulkan 1.0 or newer driver.
- Or **Linux x86-64** with a Vulkan 1.0 or newer driver.
- The GLES 3.0 reference twin needs an OpenGL ES 3.0 driver instead, and is
  selected with `+set cl_renderer gles3`.
- Your own retail *Elite Force II* data: the game's `.pk3` paks and the game DLL.
  The DLL is part of the required set purely as a proof-of-ownership check. It is
  never executed by this port.

## Install

1. Install the APK (`P:EFII`).
2. Launch it. On the first run the app asks for your retail files. Select them
   all in one go, since the picker takes a multi-selection.
3. It names anything missing and will not start until the set is complete, then
   copies the files into place. Later launches go straight into the game.

If you prefer to place the data yourself:

```sh
adb push base/. /sdcard/Android/obb/com.provenanceef2/base/
```

## Linux

**No Linux binary is provided.** Releases carry the Android APK only, so the
Linux target has to be built from source. It is the same engine and the same tree
as the Android build. A platform switch selects the glue, not a fork.

Prerequisites: a C++ toolchain, CMake 3.31.5 or newer, Ninja, and a Vulkan 1.0
or newer driver.

```sh
cd app/src/main/cpp
cmake --preset linux-gles3 && ninja -C build -j16
```

That produces, in `build/Release/`:

| file | what it is |
|---|---|
| `provenanceef2` | the client |
| `provenanceef2ded` | the dedicated server |
| `game.so` | the game module, opened at run time |
| `cgame.so` | the client-game module, opened at run time |
| `renderer_vulkan.so` | the shipping Vulkan renderer, `cl_renderer`'s default |
| `renderer_gles3.so` | the GLES 3.0 reference twin |

`game.so`, `cgame.so`, `renderer_vulkan.so` and `renderer_gles3.so` are opened
out of the client's own directory at run time, so the six belong together. The same directory also
builds offline evidence harnesses, which are not for play.

The engine reads its game data from a `base/` directory under its data root,
which defaults to the directory the client runs from. The simplest arrangement is
the retail data alongside the binaries:

```
<dir>/provenanceef2
<dir>/game.so   cgame.so   renderer_vulkan.so   renderer_gles3.so
<dir>/base/*.pk3
<dir>/base/global/*.cfg
```

If the retail installation lives somewhere more convenient, point the engine at
it instead of copying the paks about:

```sh
./provenanceef2 +set fs_basepath /path/to/Elite\ Force\ II
```

Settings, saves, demos and screenshots are written to the engine's home path,
which is kept separate from the game data.

## Known issues

This port is not fully play tested, and the campaign has not been played end to
end. What is open is listed here rather than left to discover.

- **Graphics.** The sparkle and beam effects do not yet match retail's timing
  exactly, and a few UI elements still diverge.
- **Sound.** Loop stacking, the silent menu clicks and the missing weapon fire
  sounds are fixed in the source, but none of it has been confirmed in play yet.
  Report anything that still misbehaves. Audio stutter under load remains open.
- **Game mechanics.** Some routines reconstructed from the retail binaries still
  have unresolved details, among them doors, the campaign-start script variables,
  and the per-class footstep, jump, pain and tricorder sounds, so a few
  behaviours may differ from retail.
- **Multiplayer.** Holomatch runs with bots. The online service the original game
  used is long dead and the vendored service glue does not work yet, and
  multiplayer across separate devices or over a network has not been exercised.
- **Saves.** Retail PC saves are not guaranteed to load here, and saves written
  by this port are not guaranteed to load in retail PC Elite Force II. Back your
  saves up.

If you hit something not on this list, please report it.

## Build

Everything needed to build is in this repository: engine, game code, renderer,
SDL2 and the other vendored libraries. No game data is needed to build, only to
run.

Android prerequisites: Android SDK (`android-35`), NDK `27.0.12077973`, CMake
`3.31.5`, JDK 17+, Gradle 8.13 (the wrapper pins it). Point Gradle at your SDK
with a gitignored `local.properties`:

```sh
echo "sdk.dir=$HOME/Android/sdk" > local.properties
app/build_apk.sh assembleDebug          # output: app/build/outputs/apk/debug/app-debug.apk
app/build_apk.sh assembleRelease        # the release variant
```

The Linux build is in [Linux](#linux) above. Its output lands in
`build/Release/`. The gate for every change is a clean rebuild at
0 warnings and 0 errors, and `ldd -r` reporting 0 undefined symbols on all six.

## Documentation

Start at `docs/ARCHITECTURE.md`. `docs/BUILD.md` covers both targets,
`docs/RENDERER.md` the renderer, `docs/PROVENANCE.md` the evidence, and
`docs/ANDROID_PORT.md` the launcher, the import and the controls.

## Licence

This project is a **mixed-licence work**, and the parts are not interchangeable.

**The id Tech 3 engine lineage is GPL.** The engine derives from id Software's
Quake III Arena GPL release, via [ioquake3](https://github.com/ioquake/ioq3) and
[OpenMoHAA](https://github.com/openmoh/openmohaa), both **GPL-2.0-or-later**. The
full GPLv2 text is in `app/src/main/cpp/COPYING.txt`, and the per-file notices
and SPDX identifiers are the authoritative statement of a file's licence. If you
redistribute the engine, keep the notices intact, ship the licence and the
source, and impose no further restrictions.

**The Elite Force II game layer is not this project's to license, and is not
GPL.** *Elite Force II* never had its engine source released. Ritual did publish
the server-side game code in 2003 as the "EF2 SDK", under its own proprietary
**Limited Use Software License Agreement**, which is non-commercial, allows no
derivative works and no sublicensing, and requires the notices to stay intact. It
is reproduced verbatim in `docs/EF2-SDK-licenseAgreement.txt`. The client game
code, the UI and parts of the engine were never released in any form. They are
reconstructed here from the retail binaries, and **this project asserts no
copyright over that material and grants no rights to it.** It remains the
property of its respective rights holders. The two licences cannot be reconciled
into one, so none is claimed; `LICENSING.md` sets out the split.

**No game assets.** No maps, models, sounds, textures or other retail data are
distributed here, in source or in any release. You must own *Star Trek: Voyager:
Elite Force II* (still sold on
[GOG.com](https://www.gog.com/en/game/star_trek_elite_force_ii)) and supply your
own installation. The game cannot and will not be provided.

**No affiliation.** This is an unofficial, non-commercial fan project. It is not
affiliated with, authorised by, or endorsed by Ritual Entertainment, Activision,
id Software, or any current rights holder. *Star Trek* and all related marks,
characters and imagery are trademarks of Paramount/CBS and their respective
owners. The game and its code remain the property of their rights holders. All
trademarks are used nominatively, with no claim of ownership.

## Credits

See [CREDITS.md](CREDITS.md) for the upstream work this port builds on, and
[THIRDPARTY.md](THIRDPARTY.md) for the vendored libraries and their licences.

## AI disclosure

Parts of this port were built with the help of AI tools. They assisted with
parsing the 3D model and texture formats, the extremely complex bot and NPC AI
system, problems around the special effects, and some of the quirks of the newly
reconstructed engine and client.
