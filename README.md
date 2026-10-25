# Provenance: Elite Force II decomp (Android/Linux)

A native Android and Linux port of *Star Trek: Voyager — Elite Force II* (Ritual
Entertainment, 2003). The engine descends from id Tech 3 through
[OpenMoHAA](https://github.com/openmoh/openmohaa), with a from-scratch Vulkan
renderer written for this port. The game runs on your own retail installation —
**no game data is included**, and the DLL that ships with the game is used only
as a proof-of-ownership check. It is never executed.

Inspired by and building on [VoyagerNX](https://github.com/faithvoid/VoyagerNX),
faithvoid's Nintendo Switch port of the first game, and the sibling
[Voyager Elite Force: SP for Android](https://github.com/imjustadudegamer/VoyagerSP-Android).

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

- Vulkan renderer, written for this port — the only shipping renderer.
- Touch controls: virtual stick, look area, and on-screen pills for fire, jump,
  crouch, use, night vision and the tricorder.
- Full gamepad support (SDL GameController, EF1-style layout — see the table below),
  sharing one key map with the touch controls.
- First-run game import through the system file picker — no storage permission needed.
- Widescreen support for the 3D view, the HUD and the 2D menus.
- Android 8.0+ (`arm64-v8a`, 16 KB page-size compatible) and Linux x86-64 from one tree.

## Requirements

- Android 8.0+ with a Vulkan driver, or Linux x86-64 with a Vulkan driver.
- Your own retail *Elite Force II* data: the game's `.pk3` paks and the game DLL.

## Install

1. Install the APK from Releases and launch it.
2. On first run, pick your retail paks and the game DLL in the file picker
   (multi-select). The game copies them into place and starts.
3. Manual placement, if you prefer:

```sh
adb push base/. /sdcard/Android/obb/com.provenanceef2/base/
```

## Controls

Gamepad (EF1-style layout, remappable in the in-game menu):

| Input | Action |
|---|---|
| Left stick | Move (click: walk) |
| Right stick | Look (click: aim) |
| Right trigger | Fire |
| Left trigger | Alt-fire |
| Right shoulder | Next inventory item |
| Left shoulder | Previous inventory item |
| A | Jump |
| B | Crouch |
| X | Use |
| Y | Night vision |
| D-pad up | Reload |
| D-pad down | Context action (`+button3`) |
| D-pad left/right | Lean |
| Back | Objectives |
| Start | Menu |

Touch: virtual stick, look area, and the on-screen pills (fire, jump, crouch,
use, night vision, tricorder). Keyboard and mouse keep the retail layout
(WASD, mouse to aim and fire, wheel for inventory, `F6`/`F9` quick-save/load,
`n` night vision, `1`–`6` weapon select).

## Known issues

This port is not fully play tested and the campaign has not been played end to
end. The big stuff is listed here rather than left to discover:

- **Graphical issues** — effects, lighting and UI elements still diverge from
  retail in places; occasional texture and overlay glitches remain.
- **Game mechanic issues** — some routines reconstructed from the retail binaries
  still have unresolved details, so a few behaviours may differ from retail.
- **No PortMaster** support.
- **The OpenGL ES 3.2 renderer is broken and disabled** — Vulkan is the only
  shipping renderer.
- **The 2D main menus need a rework** to be properly usable on Android.
- **Expect crashes**, and the occasional stutter (pipeline and shader compilation
  on first use of new effects).
- **Saves**: retail PC saves are not guaranteed to load here, and saves written
  here are not guaranteed to load in retail. Back your saves up.
- **REPORT YOUR ISSUES PLEASE!**

## Build

Everything needed to build is in this repository; no game data is required to
build, only to run.

Android: SDK (`android-35`), NDK `27.0.12077973`, CMake `3.31.5`, JDK 17+,
Gradle 8.13 (wrapper pins it):

```sh
echo "sdk.dir=$HOME/Android/sdk" > local.properties
app/build_apk.sh assembleDebug          # app/build/outputs/apk/debug/app-debug.apk
```

Linux x86-64: CMake 3.31.5+, Ninja, a Vulkan driver:

```sh
cd app/src/main/cpp && cmake --preset linux-gles3 && ninja -C build -j16
```

The Linux output lands in `build/Release/` (`provenanceef2` plus `game.so`,
`cgame.so`, `renderer_vulkan.so`, `renderer_gles3.so`), and reads its game data
from a `base/` directory beside it, or `+set fs_basepath /path/to/install`.

## Licence

Mixed-licence work. The engine lineage is **GPL-2.0-or-later** (id Tech 3 via
ioquake3 and OpenMoHAA — keep the notices intact and ship the source). The Elite
Force II game layer is **not GPL**: the 2003 SDK portions carry Ritual's own
limited-use licence (`docs/EF2-SDK-licenseAgreement.txt`), and the reconstructed
client/UI code asserts no copyright and grants no rights — it remains the property
of its respective holders. **No game assets** are distributed here; you must own
the game ([GOG](https://www.gog.com/en/game/star_trek_elite_force_ii)). Unofficial
fan project, not affiliated with or endorsed by Ritual, Activision, id Software or
Paramount/CBS. See [LICENSING.md](LICENSING.md),
[CREDITS.md](CREDITS.md) and [THIRDPARTY.md](THIRDPARTY.md).

Parts of this port were built with the help of AI tools (model/texture formats,
the bot and NPC AI system, special effects, and engine/client reconstruction).
