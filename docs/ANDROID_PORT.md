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
tricorder, the radar and the rest — from the one place the 640x480 virtual space
is resolved (`CL_FillUIDef`), so it is a layout-time factor and it composes with
the pillarbox rect that same function computes. It is archived, so a config
rewrite keeps it.

| value | effect |
|-------|--------|
| `0` | auto (default) |
| `> 0` | that factor, clamped to `[1.0, 2.0]` |

Auto is `1.0` on the desktop, which leaves the Linux build pixel-identical: the
stores are multiplied by exactly `1.0f`. On Android it is the display density
over 160, clamped to `[1.0, 2.0]` — a physical-size rule, not a pixel count. A
6.4" 720p panel reports density 320 and a 6.4" 1440p panel reports 560, so both
land on the shared cap of `2.0` and the HUD reads the same size on each, while
the clamp is the same at both ends and no device ends up smaller than it is
today.

## Controls

Both input methods ship and share one key map
(`code/client/touch_controls.h`): the on-screen pills queue the same key codes
the gamepad does, so rebinding `PAD0_B` also moves the touch crouch pill. The
gamepad maps through SDL's GameController API — sticks to move/look, triggers to
fire, face buttons to jump/crouch/use/reload, D-pad to tricorder/objectives/lean,
Back to the visor, Start to the menu. While a menu is up the common buttons are
translated to the keys the menus bind.
