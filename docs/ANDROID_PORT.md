# Android port

The Android module is the launcher, the data import and the touch/gamepad
layout; the engine is the same tree as the desktop reference (see `BUILD.md`).

## Activities

| source | role |
|---|---|
| `app/src/main/java/com/provenanceef2/ImportActivity.java` | launcher: imports the retail data, then starts the engine |
| `app/src/main/java/com/provenanceef2/EF2Activity.java` | the engine (SDLActivity) |
| `app/src/main/java/org/libsdl/app/*.java` | SDL2's Java half |

## Identity

`app/src/main/cpp/VERSION` is the only place the version is written. The
CMake build feeds it to `qcommon/q_version.h`, which derives `PRODUCT_VERSION`
and `PRODUCT_VERSION_FULL`; the console banner prints the latter, and
`qcommon/common.c` registers `shortversion` as a `CVAR_ROM` cvar from the same
number. `app/build.gradle.kts` reads the same file into the manifest's
`versionName` and appends the build's commit, so two installs are
distinguishable in a manifest dump. `versionCode` is the monotone release
counter (`10500`); it is bumped when `app/src/main/assets/` changes, because an
existing install keeps its extracted copy.

## Menus are fixed up in memory, never on disk

Every menu change this port makes is an in-memory fixup applied at menu-open
time in `uilib/uimenu.cpp`. The shipped `.urc` files and every retail pak stay
byte-identical.

The main menu's lower-left version corner is the worked example. `menu_main.urc`
ships the corner as a pair of `serpentineLight-10` labels: the literal `EF2`,
and the `shortversion` label beside it. Because this port's `shortversion` is
its own number, retail's text is replaced in place rather than stacked beside:
`MENUVersionCornerFixup` re-titles the first label to the same string the
console banner prints (`P:EF2 <PRODUCT_VERSION_FULL>`), widens its frame through
the authored `rect` event -- the authored path, so a mid-session
`ChangeResolution` re-derives the widened frame rather than the narrow one the
file shipped -- and disables the second. Nothing is created; the widget's own
font, colours and border survive, and the label draws clipped to its own frame.

The Android video page carries the same discipline. `MENU3DAndroidVideoMenuFixup`
reuses the resolution `SwitchBox` as the aspect selector -- `Fill`, `16:9`,
`4:3`, driving the `android_aspect` cvar -- hides the desktop resolution list,
and retitles the row. `MENU3DAndroidAdvancedVideoMenuFixup` hides the two
antialiasing checkboxes (`r_arb_multisample`,
`r_ext_nv_multisample_filter`), which are registered and read nowhere, so they
change neither picture nor cost. The Advanced Video page's own max set is pinned
by the defaults seed below: `r_flares 2` (the page's top option; the lane tests
the value, so 1 and 2 draw identically) and `r_dynamiclightmaps 1` are the two
rows that need an explicit seta, because every other page cvar is born at its
maximum in `renderervulkan/tr_init.c`. `r_dynamiclightmaps` is display only --
the renderer declares it, neither defines nor reads it -- so without the row its
checkbox would read an unregistered cvar and show unchecked. Shadows stays on
`cg_shadows 1`, the simple path: this lane's depth format carries no stencil, so
the stencil shadow path renders nothing.

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

`app/src/main/assets/base/zpak-android.pk3` carries the on-screen touch art, the
port's front-end text override, and `global/default.cfg` /
`global/defaultcontrols.cfg` / `global/defaultoptions.cfg`, all generated from
`code/qcommon/ef2_android_defaults.h` by `app/make_zpak.py` so they cannot drift
from the engine's own first-boot defaults. The two `defaultcontrols` /
`defaultoptions` names are the payloads the shipped "Restore Controls" /
"Restore Defaults" prompt menus exec. `global/default.cfg` is kept packed even
though the current engine has no reader for it: the pak is read by whatever
binary the launcher points at, and binaries up to `c8d4dd90` still exec that path
and still `Com_Error` on it in `FS_InitFilesystem`/`FS_Restart`. Dropping the
entry broke exactly that (a stale binary plus this pak aborted with
`Com_Error: FATAL: Couldn't load default.cfg` at 2026-10-15 09:03).

`zpak-android.pk3` is the only pack the build ever regenerates. `app/make_zpak.py`
rebuilds it from the header text; `--check` verifies the pak is current and
exits non-zero when it is stale.

## Configuration

Exactly ONE config is loaded at launch: `config.cfg` in the engine's write root.
It holds every setting and every key binding, is exec'd at every boot and
rewritten on exit. The name is the compile-time `EF2_CONFIG_FILE`; the dedicated
server uses `config_server.cfg`. No `global/default.cfg` is exec'd: that is the
defaults file, the loose-before-paks copy of it grew the Touch Controls section
in the Linux settings menu, and the compiled seed replaces it.

One boot cfg is exec'd and it is not a defaults file: `global/game.cfg` ships
inside retail's pak0 and carries the game-side `setd` values the engine has
nowhere else -- `introCommand` (the only thing that starts the intro RoQs),
`ui_mainMenuModel`, `cg_3rd_person`, `r_znear`, `g_statefile`, `g_playermodel`
and the `mp_*` defaults. It is applied before `config.cfg`, exactly where retail
applies it, so the archive wins every value it names.

The first-boot defaults are command text in the binary
(`EF2_ANDROID_DEFAULT_COMMANDS`, in `code/qcommon/ef2_android_defaults.h`)
applied once per install and persisted straight into `config.cfg`, on both
platforms and from one source. The gate is `PORT_DEFAULTS_SEED` in
`qcommon/common.c`, a compile-time constant compared against the archived
`port_defaultsSeed` cvar, which only this code writes: the old gate read the
Android install's own `in_androidDefaults`, and a stray value in a shared write
root silently outran the constant, so the defaults were never folded in at all.
Bump `PORT_DEFAULTS_SEED` whenever the defaults header changes, or an install
that already archived the previous set never sees the new rows.

There is no `autoexec.cfg`: a per-machine override belongs in `config.cfg`.

The defaults carry the frame and input baseline the port ships: `com_maxfps 60`,
`r_swapInterval 0`, `r_smp 1`, `r_jobs -1` and `cl_jobs -1` (the job pools),
`r_vboStatic 0`, and the max-detail graphics set (`r_picmip 0`, `r_lodbias 1` --
which is the maximum here, not 0: `tr_model.cpp`/`tr_surface.c` return every
vertex once the value clears 0.5, so 0 enables the distance collapse --
`r_subdivisions 2`, `r_texturebits 0`, `r_colorbits 32`, `r_flares 2`,
`r_dynamiclightmaps 1`).

### Dialogue level, deliberately not retail's

The dialogue bus is the one audio default this port does not take from retail.
Retail registers `s_volume 1.0` and `s_musicvolume`/`s_dialogvolume`/
`s_sfxvolume`/`s_combatvolume` all at 0.60, and the bus is
`master x category x authored`, raised to `5/3` by Miles' own curve. On the
tutorial pair the shipped data authors `lcars_hit.wav` at 1.6 and Tuvok's line at
1.0, so retail gives the effect 0.934226 against dialogue 0.426827 -- a factor of
2.188769, +6.804 dB, which is the imbalance the mix was reported for.

The dialogue bus therefore defaults to **0.96**, the value that carries
`s_sfxvolume x 1.6 / 1.0` and puts dialogue exactly at that effect instead of
under it (both resolve to 0.934226, ratio 1.000000, 0.000 dB). `s_sfxvolume`
stays at retail's 0.60: raising dialogue alone meets the target, so the effects
side is untouched. `s_dialogvolume` is `CVAR_ARCHIVE`, so the value also ships as
`seta s_dialogvolume 0.96` in `EF2_ANDROID_DEFAULT_COMMANDS` (with
`PORT_DEFAULTS_SEED` bumped) for an install whose `config.cfg` already
archived retail's 0.60. Both gains are computed from the port's own source
arithmetic, before and after; the default must not be "corrected" back to 0.60.

### The mixer

`client/snd_mixer.cpp` walks the channel pool once per frame and applies one
gain per bus: DIALOG, MOVIE, MUSIC, AMBIENT, SFX and UI. COMBAT is folded into
SFX (its cvar stays registered and aliases onto the SFX trim) and AMBIENT is the
looping-sound pool, which keeps the SFX trim and takes the SFX depth. DIALOG and
MOVIE are protected: cinematics carry lip-synced speech.

An analytic sidechain ducks MUSIC while a dialog plays. Ten-millisecond blocks
of K-shaped RMS are measured once per sound, while its PCM is in hand at upload,
and pooled; the detector runs on the pool rather than on the samples. Attack
12 ms, hold 120 ms, release 350 ms, and the music bus drops by
`s_duckDepthMusic` (6 dB) while the dialog envelope is open. A loudness anchor
and a limiter sit behind the envelope; the anchor re-centres each bus on its own
measured loudness, so a quiet bus is not mistaken for a ducked one.

`s_duckEnable` is `1` by default and `0` makes every contribution a no-op, which
is the pre-mixer behaviour bit for bit. The desktop launcher flips it with
`--legacy-audio` (the launcher script is untracked; see `BUILD.md`).

## In-game HUD scale

`ui_hudScale` scales the in-game HUD, weapon ammo, the dialogue box, the
tricorder, the radar and the rest; and nothing else. The 640x480 virtual space
is still resolved by `CL_FillUIDef` as the retail division of the pillarbox rect,
so every 2D menu, dialog, loading screen and scoreboard is pixel-identical. The
factor is a scope around a HUD element's own subtree, and it is applied once: a
HUD element is authored `noresize`, so `ChangeResolution` lays its authored pixel
frame out multiplied by the factor, `Display` draws the subtree with the factor
reaching its text, and the scope is closed before anything else in the frame
resolves it. The shared virtual space itself is never multiplied, so a HUD
element that already follows the display division, the inventory bar, is not
rescaled a second time. Each HUD menu keeps the edge its `align` declares, so a
`bottom left` element grows up and right from the bottom-left corner, a `top
right` element grows down and left, and a centred element grows about its centre.
It is archived, so a config rewrite keeps it.

Two sets are in scope. `hudList` holds everything `ui_addhud` pushes there,
including the menus the game adds by script through `Player::addHud`, the
tricorder's keypad, modulation and route panels among them, with the
multiplayer scoreboard (`dm_scoreboard`) excluded. The client-managed set is
scaled directly: the weapon-ammo readouts (`weaphud*`) and the dialogue/comm box
(`headhud`, `headtexthud`, `texthud`), plus the small HUD panels. A HUD menu
authored across the whole 640x480 screen is an overlay: its edges and geometry
stay where retail puts them, so nothing is pushed past the screen, and the factor
reaches its text through the draw scope. A smaller panel has room to grow, so its
geometry scales and keeps its coarse corner.

A scaled HUD element's frame is in scaled pixels, so the font that measures and
draws its text carries the same factor: `ChangeResolution` tells the widget's
`UIFont` the factor at the moment it chooses the frame, and every metric read ,
at layout, at a print event or at the draw, answers in that unit. The dialogue
console is why: it derives its wrap width from its frame at layout time and its
character widths when the print event arrives, and a metric that resolved through
the draw scope answered in authored pixels outside it, so a long line never
wrapped. `UIFont::Print` divides the origin by the factor and hands the factor
to `Rend_DrawString`, which multiplies the glyph's coordinates by it, so the flag
scales the glyphs and not the string's origin, retail's own reading of it
(`UIFont::Print` @0x0005FB70, `R_DrawString` @0x0000D250). A font no widget has
laid out still follows the draw scope.

| value | effect |
|-------|--------|
| `0` | auto (default) |
| `> 0` | that factor, clamped to `[1.0, 2.0]` |

Auto is `1.0` on the desktop, which leaves the Linux build pixel-identical: no
scope is entered at all at `1.0`. On Android it is the display density over 160,
clamped to `[1.0, 2.0]`, a physical-size rule, not a pixel count. A 6.4" 720p
panel reports density 320 and a 6.4" 1440p panel reports 560, so both land on the
shared cap of `2.0` and the HUD reads the same size on each, while the clamp is
the same at both ends and no device ends up smaller than it is today.

The density comes from `SDL_GetDisplayDPI` first; because that query can fail on
a device, the Resources density is read over JNI (`Sys_AndroidDensityDpi`) as the
second source, and a phone floor (320 dpi) is used if neither answers, so the
feature cannot silently collapse to `1.0`. The effective factor is printed once
at startup on every platform, with the path it came from, the raw density and the
pre-clamp ratio.

The touch overlay's own geometry is separate from the HUD scale: the pill layout
resolves against the device's display aspect (`IN_TouchAspect`), so a wide panel
spreads the same control set instead of stretching a portrait arrangement.

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
gamepad maps through SDL's GameController API, sticks to move/look, triggers to
fire, face buttons to jump/crouch/use/reload, D-pad to tricorder/objectives/lean,
Back to the visor, Start to the menu. While a menu is up the common buttons are
translated to the keys the menus bind.

Stick feel is the EF1 Android port's answer key, carried in the defaults seed:
`cl_yawspeed 245`, `cl_pitchspeed 180` (full stick deflection, degrees per
second), `sensitivity 5.0`, `m_pitch 0.022`, `joy_threshold 0.15`,
`cl_touchLook 2.2`. `in_joystickUseAnalog` stays 0 because this engine has no
analog consumer for it -- `cl.joystickAxis` is written and read nowhere and
there is no joystick movement handler -- so EF1's 1 would route the movement
stick into a dead axis array and the player would not move.

The touch page carries one slider for look sensitivity, one for the axis
deadzone, and the crouch-toggle checkbox. The size and fade rows are gone:
`cl_touchScale` and `cl_touchOpacity` only size and fade the on-screen buttons,
which is not what the page was being asked to fix, and `joy_threshold` is the
axis deadzone itself. The deadzone slider starts at the EF1 floor (0.05) and
puts the shipped default (0.15) a third of the way up.

## Networking and permissions

The manifest declares zero permissions. Every `<uses-feature>` entry is
optional, and game data lives in the app's own OBB directory, which needs no
storage permission at any target SDK level. Because the app never joins
Android's inet group, `socket()` returns `EACCES` -- which is the condition the
networking path handles rather than hides.

Single player never touches a socket: the client and the server talk through the
in-memory `loopbacks[]` queues in `qcommon/net_chan.c`. `NET_Init` builds only
the local address table; the real UDP socket opens lazily, on the first packet
aimed at a non-loopback address. The open is attempted once per networking
session and the refusal is sticky, so a denied socket prints one row
(`NET: remote play unavailable -- no UDP socket ...`) instead of a warning per
packet for as long as the game runs. A remote connect asks
`NET_RemoteAvailable` before committing, so a refused socket fails clean: the
client disconnects, returns to the main menu and opens the engine's own ok
dialog rather than sitting in a retransmit loop. Single player is unaffected.

## Logging and measurement

Every engine instrument answers to one master switch, `qa_probe`, registered at
0: the Android build boots silent, and stays silent unless a crash prints or
something arms a probe family explicitly. There is no ambient logging on the
device.

The desktop launcher (untracked, `personal/scripts/play_ef2_decomp.sh`) arms the
instrument set with the qa_probe arm and disarms it with `--quiet` or `--clean`. A
probe-armed boot runs under the Vulkan validation layer with every probe family,
the engine log and the host monitors on, so its frame timings are not
performance numbers: the measurements it takes are the cost of being measured.
Any performance claim is taken from a quiet boot.
