# Android port

The Android module is the launcher, the data import and the touch/gamepad
layout; the engine is the same tree as the desktop reference (see `BUILD.md`).

## Activities

| source | role |
|---|---|
| `app/src/main/java/com/provenanceef2/ImportActivity.java` | launcher: imports the retail data, then starts the engine |
| `app/src/main/java/com/provenanceef2/EF2Activity.java` | the engine (SDLActivity) |
| `app/src/main/java/org/libsdl/app/*.java` | SDL2's Java half |

## Game data (first run)

The import screen asks for the retail files and copies them into `<obb>/base/`
through the Storage Access Framework, the only route that works on Android 11+.
It names every missing file and refuses to start until the set is complete,
because a partial import can boot to a map with missing art or sound. The paks
are ~1.1 GB and the picker takes a multi-selection, so they import in one go. On
devices where the shell can reach the directory:

```sh
adb push base/. /sdcard/Android/obb/com.provenanceef2/base/
```

`gamex86.dll` is part of the set as the proof-of-ownership check; it is never
executed.

## Assets

`app/src/main/assets/base/zpak-android.pk3` carries the on-screen touch art and
the port's own `global/default.cfg`, `global/defaultcontrols.cfg` and
`global/defaultoptions.cfg`, generated from
`code/qcommon/ef2_android_defaults.h` by `app/make_zpak.py`. The engine
hard-requires `global/default.cfg`; keeping it in the pak leaves `config.cfg` as
the one loose cfg. A retail install's loose `base/global/default.cfg` still wins,
because loose directories search before the paks.

## Configuration

Exactly one loose cfg exists on disk: `config.cfg` in the engine's write root.
It holds every setting and every key binding, is exec'd at every boot and
rewritten on exit. The name is the compile-time `EF2_CONFIG_FILE`; the dedicated
server uses `config_server.cfg`. The first-boot defaults are command text in the
binary (`EF2_ANDROID_DEFAULT_COMMANDS`) applied once per
`ANDROID_DEFAULTS_VERSION` and persisted straight into `config.cfg`. Bump that
constant whenever the defaults header changes.

There is no `autoexec.cfg`: a per-machine override belongs in `config.cfg`.

### In-game HUD scale

`ui_hudScale` scales the in-game HUD — weapon ammo, the dialogue box, the
tricorder, the radar and the rest — and nothing else. The 640x480 virtual space
is still resolved by `CL_FillUIDef` as the retail division of the pillarbox rect,
so every 2D menu, dialog, loading screen and scoreboard is pixel-identical. The
factor is a scope around a HUD element's own subtree: the widget set's
`ChangeResolution` lays out a HUD menu with the pair multiplied, `Display` draws
it that way, and the pair is restored before anything else in the frame resolves
it. Each HUD menu keeps the edge its `align` declares, so a `bottom left` element
grows up and right from the bottom-left corner, a `top right` element grows down
and left, and a centred element grows about its centre. It is archived, so a
config rewrite keeps it.

Two sets are in scope. `hudList` holds everything `ui_addhud` pushes there,
including the menus the game adds by script through `Player::addHud` — the
tricorder's keypad, modulation and route panels among them — with the
multiplayer scoreboard (`dm_scoreboard`) excluded. The client-managed set is
scaled directly: the weapon-ammo readouts (`weaphud*`) and the dialogue/comm box
(`headhud`, `headtexthud`, `texthud`), plus the small HUD panels. A HUD menu
authored across the whole 640x480 screen is an overlay: its edges and geometry
stay where retail puts them, so nothing is pushed past the screen, and the factor
reaches its text through the draw scope. A smaller panel has room to grow, so its
geometry scales and keeps its coarse corner.

| value | effect |
|-------|--------|
| `0` | auto (default) |
| `> 0` | that factor, clamped to `[1.0, 2.0]` |

Auto is `1.0` on the desktop, which leaves the Linux build pixel-identical: no
scope is entered at all at `1.0`. On Android it is the display density over 160,
clamped to `[1.0, 2.0]` — a physical-size rule, not a pixel count. A 6.4" 720p
panel reports density 320 and a 6.4" 1440p panel reports 560, so both land on the
shared cap of `2.0` and the HUD reads the same size on each, while the clamp is
the same at both ends and no device ends up smaller than it is today.

The density comes from `SDL_GetDisplayDPI` first; because that query can fail on
a device, the Resources density is read over JNI (`Sys_AndroidDensityDpi`) as the
second source, and a phone floor (320 dpi) is used if neither answers, so the
feature cannot silently collapse to `1.0`. The effective factor is printed once
at startup on every platform, with the path it came from, the raw density and the
pre-clamp ratio.

### Pointer

The software cursor is drawn only where a pointer exists. On Android a touch or a
gamepad reaches the UI but is not a pointer, so nothing is drawn until SDL
delivers a genuine mouse event (`which` other than `SDL_TOUCH_MOUSEID`); once one
arrives the cursor appears, so a mouse attached to the device works. On the
desktop the draw is unconditional, exactly as retail.

## Controls

Both input methods ship and share one key map
(`code/client/touch_controls.h`): the on-screen pills queue the same key codes
the gamepad does, so rebinding `PAD0_B` also moves the touch crouch pill. The
gamepad maps through SDL's GameController API — sticks to move/look, triggers to
fire, face buttons to jump/crouch/use/reload, D-pad to tricorder/objectives/lean,
Back to the visor, Start to the menu. While a menu is up the common buttons are
translated to the keys the menus bind.
