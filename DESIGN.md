# DESIGN — the settings menu, rebuilt on EF2's own LCARS look

Branch `settings-menu`, base `5a461d57`. The two pages this lane owns are `video` and
`controls`; the other two settings pages (`audio`, `GameOptions`) stay retail's own and keep
the rows they already carry.

## 0. What the reference actually does (EF1, the operator's own SP port)

Read exhaustively at `/home/adam/Desktop/PORTS/VoyagerNX_Android/android-port-Single-Player/efgame/src/ui/`:

* **Graphics** (`ui_video.cpp`, `VideoData_MenuInit` @1081, `ApplyChanges` @773): one page,
  sectioned by LCARS panel boxes, with a **quality preset chooser** at the top
  (`s_ivo_templates` @312: High / Normal / Fast / Fastest / Custom) that fills the individual
  rows; the individual rows are **spincontrols with word values** (Low/Medium/High…), never raw
  numbers; an **Apply Changes** action at (501,245) that writes the cvars and runs `vid_restart`;
  a `$$footNoteEnum` description bar. EF1 **cut** the GL-era rows and said why in the file
  (`ui_video.cpp:1204-1207` — "GL Driver / GL Extensions options removed … GL-era cvars, inert
  on the Vulkan renderervk backend"), and commented out the lighting row.
* **Controls** (`ui_controls2.cpp`): bind lists grouped by section
  (`g_controls[]` @422: Attack / Command / Attack&Look / Move / Mouse / Other), a **rebind
  flow** (`Controls_ActionEvent` @1363: activate → `g_waitingforkey`, a blinking "waiting for
  key" row, and `ui_bindGrab 1` so a pad button is captured raw instead of becoming
  ENTER/BACK), **sliders with integer notches** (`sensitivity` 2..30, `cl_anglespeedkey` 1..5,
  `gp_lookSens` 10 notches → 0.25..2.5), and the note the EF1 port left at
  `ui_controls2.cpp:2903`: *the generic slider arrow step is a hardcoded ±1, a float range
  spans fewer than two steps so any d-pad nudge slams the thumb to an end — use integer
  notches and map to the float cvar in the callback*.
* **Navigation** (`ui_qmenu.cpp` `Menu_DefaultKey` @2880): ESC/MOUSE2 pops; UP/DOWN/TAB walk;
  ENTER and the pad buttons activate; the focused widget takes LEFT/RIGHT first
  (`Slider_Key` @1836 steps, `SpinControl_Key` cycles); a click activates what is under the
  cursor.

## 1. What this port already has (and what was missing)

* The uilib widget set, the LCARS palette from the shipped data (row label navy
  `0.122 0.157 0.251`, panel border `0.278 0.365 0.510`, control orange `0.953 0.667 0.271`
  — `menu_video.urc`/`menu_advancedvideo.urc`), `menu/generic/button-hover`,
  `menu/generic/button-press`, `textures/menu/checkbox_checked|unchecked`,
  `textures/menu/slider_thumb|_sel`, `textures/menu/pulldownarrow|_sel`,
  `textures/menu/presskey`.
* The pad walk (`UIWidgetContainer::moveFocus`, `uiwidget.cpp:4970`), the pad's activation
  (`UIWindowManager::ActivateFocusedWidget`), the pad's row hand-off, `[UIVK:padnav]`.
* Touch: `in_guimouse` routes a finger to `CL_SetMousePos`, so a tap is a click and a slider
  drag is a drag.
* The bind grab is already pad-complete: `sdl_input.c` `IN_GamepadMenuKey` hands back the raw
  `K_PAD0_*` while `UI_BindActive_C()` (`cl_ui.cpp:1641`), and
  `UIWindowManager::KeyEvent` routes everything to `m_bindactive` first.
* **Missing**: the controls page's bind apparatus is hidden wholesale on Android —
  `s_adaptBindWidgets` (the four tables, the four tabs, `PressKey` and the table templates)
  plus `ClearShowCommands`, so the page's own showcommands could not re-arm the Weapons tab —
  and the bind data is never loaded: retail reads `ui/bind.scr` out of `locpak1.pk3` and this
  port's `bind_t client_bind` (`cl_ui.cpp:128`) is declared and never filled, and nothing
  calls `BindListener`. The handheld had **no rebinding at all**. The rows themselves turned
  out to be in the page data (`menu_controls.urc` ships all 40 as `addrow`), so the fix is to
  stop hiding them, not to author them. A touch tap could not arm a bind row either:
  `UIBindButton::Pressed` was empty (`uibind.cpp:301`), retail's own flow is keyboard-only.

## 2. The graphics page (`video`)

Rebuilt in code at menu load, from one declarative row table, in the shipped page's container.
Retail's rows are hidden (`Disable` → hide), never deleted from the pak. The page's own panel
boxes and headers are re-authored to the new section list.

Row geometry (authored 640x480): full-width rows, label x=53 w=250, control x=313 w=270,
16 px pitch; section headers 14 px; content y=85..393; the shipped Apply (424,401) and
Main Menu (435,428) buttons stay where retail put them.

### 2.1 Kept

| section | row | widget | cvar | values |
|---|---|---|---|---|
| Display & Quality | Aspect | SwitchBox | `android_aspect` | Fill / 16:9 / 4:3 |
| | FPS Counter | CheckBox | `cg_drawFPS` | the corner readout, on by default |
| | Frame Rate Limit | SwitchBox | `com_maxfps` | Off 0 / 60 / 120 (see section 6) |
| | Brightness | Slider 0.5..3.0 step 0.1 | `r_gamma` | live |
| | Texture Quality | SwitchBox | `r_picmip` | Low 2 / Medium 1 / High 0 (LATCH → Apply restarts) |
| | Geometry Detail | SwitchBox | `r_subdivisions` | Low 8 / Medium 4 / High 2 (LATCH → Apply restarts) |
| Lighting | Simple Lighting | CheckBox | `r_light_simple` | live (`tr_sphere_shade.cpp:790` gates RF_EXTRALIGHT on it) |
| | Simple Lighting Reach | SwitchBox | `r_light_simple_dist` | Close 100 / Near 2500 / Far 5000 |
| | Vertex Lighting | CheckBox | `r_vertexLight` | LATCH (`tr_bsp.c:370`) |
| | Dynamic Lights | CheckBox | `r_dynamiclight` | live (`tr_scene.c:437`) |
| | Shadows | SwitchBox | `cg_shadows` | None 0 / Simple 1 |
| | Flares | SwitchBox | `r_flares` | None 0 / On 1 |
| Effects & Detail | Effect Detail | SwitchBox | `cg_effectdetail` | Low 0.5 / Medium 0.8 / High 1.0 |
| | Curve Detail | SwitchBox | `r_lodCurveError` | Low 10 / Medium 125 / High 250 |
| | Deform Geometry | CheckBox | `r_allowGeometryDeforms` | live (`tr_shade_calc.c:701`) |
| | Detail Surfaces | CheckBox | `r_detailSurfaces` | live (`tr_main.c:1040`) |
| | Detail Textures | CheckBox | `r_detailtextures` | live (`tr_shader.c:2258`) |
| | Wall Decals | CheckBox | `cg_marks` | live (`cg_marks.c:480`) |

`r_subdivisions` carries Geometry Detail alone: here `r_lodbias 1` **is** the maximum
(`tr_model.cpp:663` / `tr_surface.c:190` return every vertex once it clears 0.5, and 0 would
*enable* the distance collapse), so EF1's `r_lodBias` half of the row would be a trap and is
not offered.

### 2.2 Deleted, with the reason

| row | cvar | why it is gone |
|---|---|---|
| Video Mode + the resolution pulldown | `r_mode` | Android's surface is one fixed-orientation window; `r_mode` is pinned `-2` and the Aspect row owns the geometry. Desktop keeps retail's rows and this table. |
| Color Depth | `r_texturebits` | registered and never read; 16 would force a downgrade in `tr_image.c`. Already hidden on both targets (`s_adaptRows`). |
| Texture Filter | `r_texturemode` | no cvar is registered anywhere; `VK_TextureMode` (`tr_image.c:124`) has no caller and the image paths force the mip-capable filter pair. |
| Full Screen | `r_fullscreen` | always 1 here (LATCH, the surface is the whole window). |
| Advanced → prompt hop | — | the remaining advanced rows are inlined above; the hop cost a page transition for nothing. |
| OpenGL Extensions | `r_allowExtensions` | GL-era master gate; nothing in renderervulkan reads it. |
| Full Screen Antialiasing | `r_arb_multisample` | never read; every pipeline is `VK_SAMPLE_COUNT_1_BIT`. Already hidden. |
| Nvidia Antialiasing | `r_ext_nv_multisample_filter` | never read. Already hidden. |
| Anisotropic Filtering | `r_anisotropicFiltering` | `info.anisotropyEnable = VK_FALSE`, hard off (`vk_texture.c:1554`). |
| Monitor Sync | `r_swapInterval` | registered and honored, but the operator order is explicit: leave it alone (it is deliberately absent from the defaults). |
| Dynamic Lightmaps | `r_dynamiclightmaps` | **kept, moved** — see the note below. |
| Level of Detail Distance | `r_lodScaleDistance` | inert while `r_lodbias > 0.5`: the LOD early-out returns before it is ever read, and this menu offers no control that clears the early-out. |

Note on `r_dynamiclightmaps`: the defaults header calls it display-only, but
`renderervulkan/tr_init.c:707` reads it in `RE_SetDynamicLights` (the per-light-group
intensity gate), so the row is **kept** on the Lighting section and the stale comment is
corrected in place. Toggling it is live (not latched).

Note on the quality preset row: EF1's most-used control is the preset chooser, but its
templates here would write a *lower* quality over the operator's max-graphics pins on an
install that only touched the preset. It is deliberately **not** carried; Restore Defaults is
the one way back to the pins.

## 3. The controls page

Retail's own page structure, fed and finished:

* The **five tabs** are the section switcher: Weapons / Combat / Move / Game / Touch.
  The four shipped tab buttons are retitled, narrowed to a single 5-slot row (64 px each) and
  their `stuffcommand` strings rewritten to enable exactly one panel; a fifth tab is built in
  code. `Enable`/`Disable` is the show/hide mechanism (`uiwidget.cpp:2883`/`:2897`), so the
  shipped `globalwidgetcommand` strings keep working unchanged.
* The **four shipped bind tables** come back (`WeaponBindList`, `AttackLookBindList`,
  `MovementBindList`, `GameplayBindList`), fed once at menu load from a code table — the same
  `globalwidgetcommand <table> addrow "<label>" "<command>" "<command>"` the port already uses
  for the save list (`cl_ui.cpp:3219`). `ui/bind.scr` is never loaded; the table in code is
  the data, and it is EF1's grouping on this port's own commands.
* **Touch** arms a bind row too: `UIBindButton::Pressed` starts the grab (the pad already
  completes it — the raw `K_PAD0_*` return is wired in `sdl_input.c`). Pad BACK / ESC cancels.
* The **Touch & Stick** panel keeps the landed rows and adds the pad's own look speed.

### 3.1 Kept

The four bind tables are **retail's own rows** (`menu_controls.urc` ships them as `addrow`
data with the animation column for the preview model), so the table below is what comes back,
not anything this lane authors:

| panel | rows | commands |
|---|---|---|
| Weapons (8) | Weapon Group 1..6, Next, Previous | `warpinv 1..6`, `nextinv`, `previnv` |
| Attack/Look (8) | Attack, Alternate Attack, Use, Holster, Reload, Look Up, Look Down, Center View | `+attackleft` `+attackright` `+use` `+holster` `+reload` `+lookup` `+lookdown` `centerview` |
| Movement (12) | Forward, Backward, Turn L/R, Run/Walk, Strafe L/R, Jump, Lean L/R, Crouch, Weapon Aim | `+forward` `+back` `+left` `+right` `+speed` `+moveleft` `+moveright` `+moveup` `+leanleft` `+leanright` `+movedown` `+aim` |
| Gameplay (12) | Objectives, Drop Rune, Message, Team Message, Taunt 1/2, Vote Yes/No, Night Vision, Pause, Quick Save, Quick Load | `+objectives_score` `+droprune` `messagemode` `teammessagemode` `taunt 1` `taunt 2` `vote y` `vote n` `nightvision` `pause` `ui_quicksave` `ui_quickload` |

**Touch & Stick** (the fifth panel, six rows this lane authors):

| row | widget | cvar |
|---|---|---|
| Touch Look Sensitivity | Slider 0.5..6.0 step 0.1 | `cl_touchLook` |
| Joystick Deadzone | Slider 0.05..0.35 step 0.01 | `joy_threshold` |
| Turn Speed | Slider 60..360 step 15 | `cl_yawspeed` (the pad's yaw rate, the key-turn path) |
| Look Speed | Slider 45..270 step 15 | `cl_pitchspeed` |
| Crouch Toggle | CheckBox | `cl_crouchToggle` |
| Easy Jump | CheckBox | `cl_easyJump` |

Mouse settings (Sensitivity `sensitivity`, Invert Mouse, Smooth Mouse `m_filter`, Weapon Aim
`m_aim`) and the gameplay rows (Always Run `cl_run`, View Bob `cg_viewBob`, View Kick
`cg_viewKick`, Auto Switch, Crosshair `cg_crosshair`, Dialog Text, Radar View) stay on the
shipped `GameOptions` page, which is where retail already puts them and which is already
adapted and navigable.

**Invert Mouse is rewired, not deleted**: retail's checkbox links `m_invert_pitch`, which this
port only *writes* from the sign of `m_pitch` (`cl_main.cpp:2334`) and nothing reads — the
real knob is the sign of `m_pitch`. EF1's answer key is the same: invert ⇔ `m_pitch < 0`.
That rewiring is left to its own lane; the row is *not* silently left dead (it is retail's own
row on a page this lane did not rebuild).

### 3.2 Deleted, with the reason

| row | why it is gone |
|---|---|
| The bind scrollbar (`bindscroll`) | the tables carry their own row walk; the pad walks rows to the end and hands back. It is not deleted from the page — it is the tables' own template and `scrollBarTemplate` disables it — it simply has no row of its own. |
| Joystick Enable (`in_joystick`) and Force Feedback (`use_ff`) | never carried: retail's page has no such rows and this lane adds none. |

Nothing else was removed from the controls page.  The player preview model stays (retail's
own, and EF1 keeps one too).

## 4. Navigation spec

* **Touch** — a tap is a click (`in_guimouse`): tap a tab to switch, tap a CheckBox to toggle,
  tap a SwitchBox to cycle, press and drag on a slider thumb to set it, tap a bind row to arm
  it and press the pad (or a key) to assign; a second tap on the row that holds the grab
  releases it, which is the escape a pad has no equal for once its buttons have gone raw
  (`IN_GamepadMenuKey` hands back the raw keycodes while `UI_BindActive_C()`). Tap Back /
  Apply as retail drew them.
* **SDL controller** — the landed walk: UP/DOWN (or the stick) moves focus in screen order,
  ENTER activates, BACK/ESC leaves, LEFT/RIGHT are the focused row's own (a SwitchBox cycles,
  a Slider steps by its `stepsize`). `ui_bindGrab`'s equivalent (`UI_BindActive_C`) keeps the
  pad's buttons raw while a bind row is armed. Every new row is a `CanActivate` widget or a
  `UISlider`, which is exactly what `moveFocus` stops on.

## 5. Detectors (all `qa_probe`-gated)

* `[UIVK:settings]` `op=hide menu='video' hidden=N` — the page's own rows going away.
* `[UIVK:settings]` `op=build menu= rows= bottom=` — once per rebuilt page.
* `[UIVK:settings]` `op=rebind cmd= key= alt= -> 'KeyName'` — a bind row assigned.
* `[UIVK:slider]` `cvar= old= new= step=` — every slider write, throttled; this covers the
  landed touch rows too, which had no row of their own.
* `[PROBE:limit]` `maxfps= budget=msec measured=msec` — the limiter engaged and the frame
  period it is actually producing, so a boot shows 120 capping to 120 and 60 to 60, and a
  panel that cannot run that fast shows it here instead of the row claiming it.
* `[UI:switchopt]` `cvar= cmd= label=` — every SwitchBox selection (the existing row, now
  naming the label and the value).
* A tab switch is named by the `[INV:gwc]` row `UI_GlobalWidgetCommand_f` already prints for
  every `globalwidgetcommand`, so the boot log shows which panel each click opened.

One armed boot proves the whole menu: open Settings → `op=hide` + `op=build` name the two
pages; walk the pad down the video page (every row names itself in `[UIVK:padnav]` `to=`),
press ENTER on a SwitchBox and on a slider; transition to Controls; switch all five tabs
(`[INV:gwc]` names each panel); arm a bind row and assign a pad button (`op=rebind`); leave.

## 6. The fix round (the operator's Pixel 8 boot, APK `0.8.0+29a405a4-dirty`)

**The leak, and the two mechanisms behind it.** The screenshot shows the video page with no
page switcher and no Apply: `UIWidget::getTitle()` hands back the *translated* string, not the
`$$token$$` the page data carries, so a keep list written in tokens matched nothing and the
rebuild disabled every widget on the page, the six that should have survived included. And
`UIWidget::PassEventToWidget` — what `globalwidgetcommand` resolves through — walks **every
menu in the tree from `uWinMan` down and the first name match wins**, so the section header and
box names this page and the controls page both authored (`SectionHead1`, `SectionBox1`) were
one name with two owners: the controls page's Touch-tab command toggled the video page's panel.
Fixed by comparing the token *and* its resolution in the keep test, and by giving every widget
this lane creates a page-unique prefix (`VidL`/`VidR`/`Touch`), so a named lookup can only ever
land on the page that sent it.

**The relayout.** One row is a label and a value, and the value is the same frame whichever of
the three it is, so the checkbox mark, the pulldown value and the slider thumb all start on the
same x; the label column is one width everywhere and the value column one width everywhere. The
rows are two aligned columns under a full-width section header — 26 px pitch, 20 px boxes — which
is what fits seventeen rows into the 640x480 page the shipped data authors, and the columns are
narrower than the old full-width rows for it. The empty bands above and below the page in the
screenshot are the page's own margins in that 480-tall space: the tab row at y=55 and the
Apply / Main Menu pair at y=401 were both missing because of the keep-list bug, and with them
back the page runs from the tab row to Main Menu. The scale itself is the port's own: the
screenshot measures x3.135 horizontally and x2.25 vertically on the 2400x1080 surface, which is
`uid.scaleRes` doing exactly what `ChangeResolution` has always done to retail's authored
coordinates; nothing in this lane assumes a surface.

**The frame-rate limiter is in, and it is EF1's.** The reference
(`VoyagerNX_Android/android-port-Single-Player/EFAndroid-SP/app/jni/efcode/qcommon/common.c`)
paces in `Com_Frame`: `minMsec` comes from `com_maxfps` with 0 = unlimited (:3248-3256), a
static `bias` adds the previous frame's overrun back into the next budget so the *average*
holds the requested rate instead of sagging (:3258-3266, "Adjust minMsec if previous frame took
too long to render so that framerate is stable at the requested value"), and the wait is
`Com_TimeVal` + `NET_Sleep(timeVal - 1)` (:3189, :3285) — sleep to one millisecond short, spin
the tail. Present is not the governor: the Vulkan path picks MAILBOX when it can and FIFO
otherwise (`renderervk/vk.c:766-781`), so the cap is the CPU's and lands on Android the same as
desktop. Adopted here: the same `minMsec` selection (this port's `common.c`, where the Android branch
had compiled the limiter out entirely — `minMsec = 1`, "uncapped by order"), the same bias
correction clamped to `[0, minMsec]`, and this engine's own `LIMIT_WaitDecision` + event-loop spin in
place of EF1's `NET_Sleep`, which is the same sleep-most/spin-the-tail shape with
`com_frameWaitMargin` for the margin. Adapted: present mode is this port's own swapchain and
`r_swapInterval` stays untouched by operator order, so the proof row names the *measured*
period and the display's ceiling is visible in it rather than claimed past.

**Superseded by the paragraph above** — the earlier round found the limiter compiled out
and shipped no row; the operator's standing order is that the limiter itself work, so it now
does (`Frame Rate Limit`, `com_maxfps` Off / 60 / 120, in Display & Quality). What that audit
turned up and still stands:: `common.c:2800` compiles the limiter out on Android —
`minMsec = 1` with the comment *"Android: uncapped by order … the archived com_maxfps 85 is not
read here … leaves the swap as the only governor"*. the compile-out was the whole reason a row would have been dead, and lifting it was the
whole job. The PC launcher pins `com_maxfps=0` every boot
(`personal/scripts/play_ef2_decomp.sh:279`), which is the easy-jump trap for a desktop row; the
handheld does not use that launcher, so the row is live where this menu ships, and the pin is
the coordinator's to drop.

**The FPS counter row is real and is wired to the cvar that actually gates the draw the
operator sees.** The corner readout is `SCR_DrawFPS` (`cl_scrn.cpp:247`, `"%ifps"` — the
"57fps" in the screenshot), gated at `cl_scrn.cpp:193` on **`cg_drawFPS`**, registered
`Cvar_Get("cg_drawFPS", "1", CVAR_ARCHIVE)` at `cl_scrn.cpp:307` — on by default, and 2 adds
the scheduler budget line. The other counter, `View3D::DrawFPS` ("FPS 57.0" plus tri/vert
stats), is a different cvar (`fps`, default 0) and stays where retail put it.

## 7. Traps this design closes by construction

* **Uninitialised `texturecoordinates`** — every row is built through `newInstance`, so the
  constructor's identity init runs; no new widget class is added.
* **The 2D window cache** — nothing here draws; the walk-thread fix in `tr_backend.c` stands.
* **Pad focus** — new rows are enabled, shown, and either `CanActivate` or a `UISlider`; the
  tab switch leaves the focus on the tab itself, so the next DOWN enters the panel.
* **The seed fold** — no new cvar is added. Every row drives a cvar that already exists on the
  unified CFG path, so the launcher's per-boot override chain and `defaultoptions.cfg` cannot
  re-pin a row out from under its own toggle.
* **Float sliders on a pad** — `cl_touchLook` and `joy_threshold` were already float and are
  the two rows the easy-jump lane shipped; they keep their steps (0.1 / 0.01), which are coarse
  enough that a pad nudge moves the thumb visibly. `cl_yawspeed` / `cl_pitchspeed` are
  integer-stepped at 15 per notch for the same reason EF1 gives at `ui_controls2.cpp:2903`.

## 8. The second round (the operator's OPSCREENSHOT2 verdict)

**No Apply.** The row goes with the keep list that protected it, and retail's own restart
test moves into the write path: `ui_settingsautoapply` compares each LATCH cvar against what
it read when the page was built (`s_settingsBaseline`, recorded in `SettingsBuildRows`), names
what moved, and stuffs `vid_restart` only when a row left its baseline — so cycling a row
back costs no restart. The three latched rows (`r_picmip`, `r_subdivisions`, `r_vertexLight`)
carry the hook on their own widgets; the checkbox's cvar write is a direct `Cvar_Set`, so the
restart always lands after the value.

**The pace is one governor.** `com_maxfps` owns it. The budget is EF1's
(`VoyagerNX_Android` common.c:3248-3266) in microseconds — 60 is 16667us, not the 16ms floor
an integer-millisecond budget gives, which is what made 62fps out of the 60 row — and the
overrun bias reads the pace clock now: the port's spin sets `lastTime` *after*
`com_frameTime`, so the old `com_frameTime - lastTime` was zero every frame and EF1's
correction never ran. On Android the swapchain stops reading `r_swapInterval`
(`vk_swapchain.c`): its compiled default is 1 (`vk_platform.c:448`) and a config that carries
it hands the pace to the display, which is the whole reason the Frame Rate row moved nothing.
EF1's own port is the answer key — `renderervk/vk.c:773-781` takes IMMEDIATE, then MAILBOX,
then FIFO_RELAXED, then FIFO for its `r_swapInterval 0` arm and lets `com_maxfps` hold the
rate. Desktop keeps retail's reading of the cvar. `[PROBE:limit]` names
`governor=budget` (the limiter held the frame) against `governor=work` (the frame's own work
did), and `VK:swapchain` names the present mode, so one armed boot says which side owns the
rate. 66fps at a 120 row is exactly this: the budget allowed 125 and the frame work took it.

**The 60 default.** The seed already carried it (`c0900cfe`, seed 3; seed 8 folded it again),
so the device's 120 was the row click, archived. The compiled default stops contradicting it
(85 → 60) and seed 9 folds it once into an install that archived something else.

**The layout.** The label column was 116px authored, which is less than the shipped string
resource resolves to for "Deform Geometry", "Detailed Textures" and "Simple Lighting Reach";
the row drew from the left and clipped. `SettingsBuildRows` now measures every label in its
own font after the build and re-issues the column at that width — one width for both columns,
so the alignment survives, and the value column keeps its right edge and gives up the
difference (`[UIVK:settings] op=labelw authored= measured=`). Rows are 22px on a 28px pitch
and the checkbox mark carries a 1.3 scale of its own (`UICheckBox::setCheckScale`), so the
mark fills the row instead of sitting 16px inside it.

**The LCARS chrome the rebuild was eating.** `background.inc` ships the page's six background
panels and its logo as `Widget` resources with a name and no title; the keep list tested
titles, so they went away with the page's own retail rows and the graphics page alone lost
its art — the starfield and the ship draw from `view3d`, every label on the page drew, and
the image quads were the widgets being disabled, not a draw the renderer refused. They are
kept by name now (`s_videoKeepNames`), and the hide row names each one it left standing with
its shader and its show state.

## 9. The graphics page is the graphics page on both targets

The operator's order: one unified graphics settings page, not an Android overlay. The
builders, the row table, the keep pass and `SettingsAdaptVideo` run on the Linux build now;
the controls page stays the handheld's (the order scoped the unification to graphics), and
the Linux build keeps its own `s_adaptRows` for the two advancedvideo rows no renderer reads.

**What the unified page replaces on Linux** — the same rows DESIGN 2.2 already deleted for
Android, which are retail's own on the shipped `menu_video.urc` both targets load:

| row | cvar | why it is gone here too |
|---|---|---|
| Video Mode + the resolution pulldown | `r_mode` | one window per surface; the Aspect row owns the geometry |
| Color Depth + `colorlist` | `r_texturebits` | registered and never read; 16 forces a downgrade in `tr_image.c` |
| Texture Filter | `r_texturemode` | no cvar is registered; `VK_TextureMode` has no caller |
| Full Screen | `r_fullscreen` | LATCH, and the surface is the whole window |
| Advanced → the advancedvideo hop | — | those rows are inlined above, so the page is gone from the walk |
| OpenGL Extensions / both antialiasing rows / Anisotropic | `r_allowExtensions`, `r_arb_multisample`, `r_ext_nv_multisample_filter`, `r_anisotropicFiltering` | nothing in renderervulkan reads any of them; every pipeline is `VK_SAMPLE_COUNT_1_BIT` |
| Monitor Sync | `r_swapInterval` | the desktop keeps the cvar (it still picks the present rung there) but it is not a row: the compositor owns the sync on both targets |
| Curve Detail | `r_lodCurveError` | **cut in this round**: `CVAR_CHEAT` (`tr_init.c:196`), and `Cvar_SetCheatState` (`cl_parse.cpp:322`, every server connect) resets it to its `resetString`, so the row could not hold what it showed. The seed's own pin is 250, which is also the registered default, so nothing else moves. |
| Dynamic Lightmaps | `r_dynamiclightmaps` | kept, on the Lighting section: `RE_SetDynamicLights` reads it (`tr_init.c:707`) |
| Level of Detail Distance | `r_lodScaleDistance` | inert while `r_lodbias > 0.5` returns before it is read |

**Every kept row, proven on the Linux/Vulkan path.** All of these consumers are in
target-neutral files, so the same file:line holds for both devices:

| row | cvar | consumer |
|---|---|---|
| Aspect | `android_aspect` | `SCR_AndroidDisplayRect` (`cl_scrn.cpp:51`), called from `cl_scrn.cpp:92/126`, `cl_input.cpp:767`, `cl_ui.cpp:989/2251` — no target gate on any of them |
| FPS Counter | `cg_drawFPS` | `cl_scrn.cpp:307` registers, `:193` gates, `:247` adds the budget line |
| Frame Rate Limit | `com_maxfps` | `Com_Frame`'s budget (`common.c`), the only pace on both targets now |
| Brightness | `r_gamma` | `R_SetColorMappings` (`tr_image.c`) → `s_gammatable` → `R_LightScaleTexture` (`tr_image.c:498`) at upload. **Not live**: the table is baked into already-uploaded textures, so a new value reaches nothing on the card. It is in the restart class now. |
| Texture Quality | `r_picmip` | `tr_init.c:170`, upload-time (`tr_image.c` scratch/registration) |
| Geometry Detail | `r_subdivisions` | `tr_init.c:192`, the patch chord tolerance (`tr_bsp.c`/`tr_curve.c`) |
| Simple Lighting | `r_light_simple` | `tr_sphere_shade.cpp:790` gates RF_EXTRALIGHT on `!r_light_simple` |
| Simple Lighting Reach | `r_light_simple_dist` | `tr_sphere_shade.cpp`, the `fDist <= value` compare |
| Vertex Lighting | `r_vertexLight` | `tr_init.c:176`, `tr_bsp.c:370` |
| Dynamic Lights | `r_dynamiclight` | `tr_init.c:202`, `tr_scene.c:437` zeroes `num_dlights` |
| Shadows | `cg_shadows` | `tr_init.c:233` registers it, `cg_specialfx.cpp:50`, `cg_modelanim.c:621` |
| Flares | `r_flares` | `tr_init.c:198`, `tr_sun_flare.cpp` tests the value |
| Effect Detail | `cg_effectdetail` | `cg_tempmodels.cpp:247` |
| Deform Geometry | `r_allowGeometryDeforms` | `tr_init.c:191`, `tr_shade_calc.c:701` |
| Detail Surfaces | `r_detailSurfaces` | `tr_init.c:190`, `tr_main.c:1040` |
| Detail Textures | `r_detailtextures` | `tr_init.c:186` (`ARCHIVE|LATCH`), `tr_shader.c:2258` |
| Wall Decals | `cg_marks` | `cg_marks.c:480/797` |

**The desktop present audit.** `r_swapInterval` still means something on the desktop and is
not read on Android (`vk_swapchain.c`): its compiled default is 1 there, so a desktop boot
without the launcher takes MAILBOX and the display caps the rate below `com_maxfps`; the
launcher pins `r_swapInterval=0` (`personal/scripts/play_ef2_decomp.sh:283`), which is the
uncapped rung, so the reference build paces exactly like the handheld — the engine's budget.
The launcher does **not** pin `com_maxfps` (the previous round's note that it pinned 0 is
stale; line 283 is `r_swapInterval`), so the Frame Rate row is live on the desktop too.

**Layout.** The page is authored in the shipped 640x480 space and scaled by `uid.scaleRes`
like every other menu, so a desktop window and the handheld surface take the same geometry;
the measured label width and the 22px rows are in authored units and scale with it.
