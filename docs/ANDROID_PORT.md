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

## Controls

Both input methods ship and share one key map
(`code/client/touch_controls.h`): the on-screen pills queue the same key codes
the gamepad does, so rebinding `PAD0_B` also moves the touch crouch pill. The
gamepad maps through SDL's GameController API — sticks to move/look, triggers to
fire, face buttons to jump/crouch/use/reload, D-pad to tricorder/objectives/lean,
Back to the visor, Start to the menu. While a menu is up the common buttons are
translated to the keys the menus bind.
