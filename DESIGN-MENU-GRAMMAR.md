# DESIGN — MENU GRAMMAR: the settings-page manual

How a shipped Elite Force II menu page is authored, how this engine turns it into widgets,
how the authored numbers reach a real surface, and how this port adds, greys or deletes a
row. Everything here is read out of the tree (`app/src/main/cpp/code/`) or out of the
shipped resources (`personal/evidence/urc/`, copied from the retail paks); every claim
carries a file:line or a parsed resource. Written for whoever tweaks a menu page next.

Shipped pages this was built from: `menu_video.urc` (704 lines), `menu_advancedvideo.urc`,
`menu_audio.urc`, `menu_gameoption.urc`, `menu_controls.urc`, plus `background.inc`.

---

## 1. What a page is

A page is one `.urc` text file. The UI loads **every** `.urc` it finds in `ui/` and in
`loc/<local_language>/ui/` at UI init, in that order, and then calls the port's adapt pass
once at the end (`cl_ui.cpp:5933-5959`):

```c
filenames = FS_ListFiles("ui/", "urc", qfalse, &nfiles);
...  new UILayout(szFilename);                       // cl_ui.cpp:5936-5940
Com_sprintf(locdir, sizeof(locdir), "loc/%s/ui/", language);
...  new UILayout(szFilename);                       // cl_ui.cpp:5948-5955
UI_AdaptSettingsMenus();                             // cl_ui.cpp:5959
```

So a page does not need registering; dropping a file in `loc/Eng/ui/` is enough, and the
adapt pass runs after every page in the tree exists.

A file is a script, parsed by `UILayout::ProcessCommands` (`uilib/uilayout.cpp:80`). Only a
handful of top-level tokens exist:

| token | what it does | where |
|---|---|---|
| `menu "<name>" <w> <h> <direction> [motiontime]` | creates the page's `UIWidgetContainer`, named `<name>`, framed `0 0 w h`, with a slide-in direction | `uilayout.cpp:57-76` |
| `bgcolor r g b a`, `borderstyle …`, `depth f` | set on the container | `uilayout.cpp` menu handling |
| `include "<file>"` | pushes the current script and parses another file in place | `uilayout.cpp:95-101` |
| `resource <Class>` `{ … }` | creates one widget (section 3) | `uilayout.cpp:109-160` |
| `end.` | stops parsing | `uilayout.cpp:89` |

Every settings page opens the same way — this is the whole page skeleton:

```
menu "video" 640 480 NONE 0        <- name, authored width, authored height, no slide-in
bgcolor 0 0 0 0.8
borderstyle NONE
depth 0.2
include "loc/Eng/ui/background.inc"  <- the LCARS frame panels + the logo
...
end.
```

`menu_video.urc:1-7`. The name is what everything else addresses: `transitionmenu video 0`
(`menu_gameoption.urc`, the Video tab's `stuffcommand`), `Menu::GetContainerWidget`, and
`UI_AdaptSettingsMenus`'s page test (`uimenu.cpp:2541`, `stricmp(container->getName(), "video")`).

`background.inc` ships six `Widget` resources (`main_bkg_01` … `main_bkg_06`) and the `logo`,
each with a **name and no title** (`personal/evidence/urc_retail2/background.inc`). That is
the "titleless widget" convention: a pure paint resource. It matters twice — it draws before
everything after it in the file, and any pass that keeps pages by *title* will drop it
(which is what the first round of this lane did; `s_videoKeepNames` in `uimenu.cpp:2082`
keeps them by name now).

---

## 2. The coordinate model

**Authored space is always 640 x 480**, whatever the surface is. `rect x y w h` is in that
space. The scale to the real surface is computed per frame in `CL_FillUIDef`
(`client/cl_ui.cpp:2288-2289`):

```c
uid.vidWidth  = dw;                 // SCR_AndroidDisplayRect, cl_scrn.cpp:51
uid.vidHeight = dh;
uid.scaleRes[0] = (float)uid.vidWidth  / 640.0f;
uid.scaleRes[1] = (float)uid.vidHeight / 480.0f;
```

`dw/dh` come from `SCR_AndroidDisplayRect` (`cl_scrn.cpp:51`): the whole surface, unless
`android_aspect` asks for 16:9 or 4:3, in which case a centred rect of that aspect. **The
rect is the handheld's only** — on the desktop the function returns the whole window
unconditionally (`cl_scrn.cpp:58-61`, and `CG_AndroidDisplayRect` the same at
`cg_view.c:40-41`), because the desktop window is already the video mode's own shape
(`r_mode` owns it, retail ships no aspect row) and a letterbox inside a window this page
cannot resize only squashes the page. Inside the rect **the two axes scale independently**
— a 1600x1000 window gives `scaleRes = (2.484, 2.066)`. Nothing in the authored data may
assume a square scale; a length measured on one axis is not valid on the other. (A 4:3
request on the handheld yields a square scale, which is why the row is live there: the
640x480 page lands undistorted inside the bars.)

A widget's authored frame is latched once and re-scaled on every resolution change
(`UIWidget::ChangeResolution`, `uilib/uiwidget.cpp:2984`):

* `m_bFrameLatched` false → `m_authoredFrame = m_frame` (the current frame *is* the authored
  one) and latched.
* then, if `isResizeable()` (`uiwidget.cpp:2962`, walks the parent chain for `m_noresize`),
  `m_frame = authoredFrame * scaleRes` — the widget's live frame is in **screen pixels**, and
  `m_authoredFrame` is in **authored units**.
* a widget that is *not* resizeable keeps its frame as authored (`uiwidget.cpp:3020-3025`);
  that is the HUD branch.

This is the single most important trap in the whole file, and it is the one this lane fell
into: **a metric that comes back from a widget is in the unit its text draws in, not the unit
its frame is authored in.** `UIWidget::getTitleWidth()` (`uiwidget.cpp:4195`) calls
`UIFont::getFloatWidth(text, -1, m_bVirtual)` (`uilib/uifont.cpp:312`), which multiplies by
`uid.scaleRes[0]` when the widget is virtual (`uifont.cpp:322-328`). On my desktop
that is x1.2536, on the Pixel 8 x3.135. Feed that straight into a `rect` and every label box is
1.25x (or 3.1x) too wide, the value column is pushed off the page, and neighbouring boxes
overlap. `SettingsTitleWidth` (`uimenu.cpp:1784`) is the port's one conversion point: measure,
then divide by `uid.scaleRes[0]` if the widget is resizeable.

Worked example, from my own screenshot (`OPSCREENSHOT3.png`, a desktop window):

```
scaleRes = (1.2536, 1.2500)      origin = (559.4, 333.75) screen px
authored header "Display & Quality"  rect 45 92 542 14
    -> screen x 559.4 + 45*1.2536 = 615.6 .. 1297.4,  y 333.75 + 92*1.25 = 448.75
    -> measured in the screenshot: text centred at 955 (authored 316 == the box centre), bar top 448
a label measured 148 px on screen is 148 / 1.2536 = 118 authored units, not 148
```

To read an authored number back out of a screenshot: `authored = (screen - origin) / scaleRes`,
with the origin taken from any widget whose authored rect you know (a section header is the
easiest: its text is centred, so the text centre is the box centre).

---

## 3. The resource grammar

```
resource
<Class>
{
    <field> <args…>
    <field> <args…>
}
```

`uilayout.cpp:109-160`:

1. the class token is translated by `Client_TranslateWidgetName` (`cl_ui.cpp:2036`), whose
   whole table is one entry — `{"Label", "FakkLabel"}` (`cl_ui.cpp:2031`) — so **a URC
   `Label` is a `UIFakkLabel`**, a `UILabel` subclass (`cl_uistd.h:48`), not a bare
   `UILabel`. Everything else passes through unchanged.
2. `"UI" + translated` is looked up with `getClass` (`uilayout.cpp:119-124`); a miss throws
   `LOAD_BAD_WIDGET` and the **rest of the file is dropped** — a page silently missing
   everything after a typo'd class name is a parse drop, not a paint drop.
3. an instance is created and `InitFrame`-ed with a placeholder.
4. each line inside the braces is dispatched **as an event**: if the class handles a command
   of that name (`widget->ValidEvent(token)`, `uilayout.cpp:137`) the rest of the line becomes
   the event's arguments. A field the class does not handle prints a warning and is ignored
   (`uilayout.cpp:148-153`). There is no schema and no error — an unhandled field is silent.

So the field list **is** the `Event` registration list of the widget's class chain. The ones
the settings pages actually use, counted over the five settings URCs:

| field | count | what it does | where it lands |
|---|---|---|---|
| `rect x y w h` | 157 | the authored frame | `UIWidget::LayoutRect`, `uiwidget.cpp:1835` |
| `title "…"` | 99 | the widget's text; `$$token$$` resolved on the way in | `setTitle`, `uiwidget.cpp:3475` |
| `name "…"` | 157 | the lookup name (`FindWidget`) | `LayoutName`, `uiwidget.cpp:1861` |
| `fgcolor`/`bgcolor r g b a` | 162/162 | per-state colours (NORMAL) | `uiwidget.cpp:1881+` |
| `hoverfgcolor`, `armedfgcolor`, `selectedfgcolor`, `disabledfgcolor` | 7/6/1/6 | the same, other states | `uiwidget.cpp:666` |
| `borderstyle`, `borderwidth`, `bordercolor` | 146/10/37 | the frame's edge | `uiwidget.cpp` |
| `shadercolor`, `hovershadercolor`, `armedshadercolor`, `selectedshadercolor`, `disabledshadercolor` | 42 each + 6 | the tint of the widget's *shader* (the checkbox mark) per state | `uiwidget.cpp:688` |
| `font <name>` | 139 | which font; also sets the text scale unit | `LayoutFont`, `uiwidget.cpp:1876` |
| `horizontaljustify`, `verticaljustify` | 77/26 | `left`/`right`/`center`, `top`/`center` | label draw, `uilabel.cpp:266` |
| `linkcvar "<cvar>"` | 48 | the cvar the row reads and writes | `LinkCvar` |
| `option "<label>" "<command>" <value>` | 57 | a SwitchBox entry | `uiswitchbox.cpp` |
| `addpopup "<menu>" "<label>" command "<cmd>"`, `popup_width` | 57/16 | the matching PulldownMenu entry | `uipulldownmenucontainer.cpp` |
| `menushader`/`selmenushader` | 16 | the pulldown's arrow art | 〃 |
| `checked_shader`/`unchecked_shader`/`hovershader`/`armedshader` | 26 | the checkbox's four states | `uicheckbox.cpp` |
| `checked_command`/`unchecked_command` | 1 | run when the mark flips | `uicheckbox.cpp:195` |
| `setrange <lo> <hi>`, `stepsize <n>`, `slidertype float\|integer` | 4 each | the slider | `uislider.cpp` |
| `stuffcommand "<cmd>"`, `hovercommand`, `hoverexitcommand`, `clicksound`, `hoversound` | 27/115/115/93/10 | the button plumbing | `uibutton.cpp` |
| `addrow …`, `tablecolumn …`, `headerheight`, `itemheight`, `norowselect`, `allowusersort`, `scrollbartemplate` | 40/20/4 each | the bind tables | `uitable.cpp` |
| `widgetstate disable\|normal\|hover\|selected\|armed` | — | the greyed state (section 7) | `uiwidget.cpp:797`, `:2110` |
| `showcommand`, `staypressed`, `ordernumber`, `cvarcheckdisable` | 5/4/4/10 | page plumbing | 〃 |

`name` values repeat wildly across pages — `Default` is the anonymous name and appears 100+
times. `FindWidget` walks **every menu in the tree from `uWinMan` down and the first name
match wins** (`UIWidget::FindWidget`, `uiwidget.cpp:4371-4390`), so a name this port creates
must be page-unique or a `globalwidgetcommand` aimed at one page will land on another. That
was the second half of the first round's leak (`SectionHead1` existed on two pages). The
convention this lane uses: every built widget carries a page prefix (`VidL`, `VidR`, `Touch`).

---

## 4. The row widget classes

One row is a label and a value. The value is one of five classes; this is which one for which
kind of setting, with the fields it reads.

| row kind | class (URC → C++) | the fields that make it a row | notes |
|---|---|---|---|
| static text, section header, description | `Label` → `UIFakkLabel` (`UILabel` subclass) | `title`, `font`, `horizontaljustify`, `fgcolor`, `bgcolor`, `borderstyle` | never activatable (`UILabel::UILabel`, `uilabel.cpp:66`) |
| on/off, or a boolean setting | `CheckBox`/`Checkbox` → `UICheckBox` | `title` (the row text — the mark is drawn at the widget's left edge, `uicheckbox.cpp:151-159`), `linkcvar`, `checked_shader`, `unchecked_shader`, `hovershader`, `armedshader`, `shadercolor` family, `checked_command`/`unchecked_command` | the mark's size is `16 * checkScale * scaleRes` (`uicheckbox.cpp:115-117`), so it scales with the surface, not the authored frame |
| a choice of 2-6 named values | `SwitchBox` → `UISwitchBox` | `linkcvar`, `option "<label>" "<command>" <value>` (repeat), `clearoptions` to empty first | a click (or ENTER, or LEFT/RIGHT) cycles; the write is the option's own command |
| a value in a range | `Slider` → `UISlider` | `linkcvar`, `setrange`, `stepsize`, `slidertype` | `command` runs on every write, not on release |
| a choice with a pulldown arrow | `SwitchBox` + `PulldownMenuContainer` | the SwitchBox shows the value; the container at `rect … 16 16` to its right opens the menu | retail's own shape: `menu_video.urc:281-339` (`reslist`) |
| a key bind row | `BindButton` → `UIBindButton` inside a `Table` | `addrow "<label>" "<cmd>" "<altcmd>"` | `menu_controls.urc`, the four tables |
| a paint-only rectangle | `Widget`/`widget` → `UIWidget` | `bgcolor`, `borderstyle`, `shader` | the section boxes and the background panels |
| a 3D model preview | `RenderModel` → `UIRenderModel` | `rendermodel`, `rendermodelangles`, `rendermodeloffset`, `rendermodelfov`, `rendermodelanimation` | the controls page's player preview |

Rows of every kind share one convention: **the label is its own widget and the value is
another.** There is no "row" object. A row is two widgets that happen to share a `y`.

The shipped geometry conventions, per page (authored units, from the parsed resources):

| page | label | value | arrow | pitch |
|---|---|---|---|---|
| `menu_video` | x 162 w 156 | x 322 w 140 | x 462 w 16 | 20 (y 108, 128, 148, 168, 188) |
| `menu_advancedvideo` | x 38 w 210 (left panel) / x 255 w 185 (right) | x 440 w 140 / x 223 w 140 | x 580 w 16 | 20 |
| `menu_audio` | x 162 w 156 | x 322 w 156 (slider) or 140 (switch) | x 462 w 16 | 20 (y 107…207) |
| `menu_gameoption` | x 38 w 156 / x 362 w 120 | x 194 w 156 / x 482 w 104 | x 586 w 16 | 20 |
| `menu_controls` | four `Table`s at `46 124 325 255`, one per tab | — | — | the table's `itemheight` |

Three things to copy from those numbers:

* **20 is the shipped row pitch, 16 is the shipped row height.** A page that needs more rows
  than fit uses a taller pitch and taller rows (this port's rebuild uses 26/26 — see the fill
  rule below — to fit eighteen rows in the same space, `uimenu.cpp:1699`).
* **a section is a pair of widgets: a `Label` header with a background colour, and a `Widget`
  box behind its rows.** `menu_video.urc:11-35` (`VideoBackground`, `rect 157 87 326 16` then
  `rect 157 103 326 105`); the header is 16 tall, the box starts 16 below the header's top and
  is 105 tall. `menu_advancedvideo.urc` uses three such pairs, two side by side
  (`rect 34 87 215 16` / `rect 251 87 350 16`).
* **the page furniture is at fixed slots**: the tab row at `y 55` (four 138x32 buttons at
  x 44, 182, 320, 458), the description bar at `rect 49 403 320 26` (`tooltip_video`,
  `tooltip_audio`, `tooltip_gameopt`, `tooltip_controls` — the hover text target), the Apply
  button at `rect 424 401 123 26` (`stuffcommand "ui_checkrestart"`), the Back/Main Menu
  button at `rect 435 428 96 32`, and an unamed `widget` at `rect 371 399 256 32` holding the
  `menu/main/credits` shader. Anything a rebuild adds must fit between the tab row and the
  description bar: **authored y 87 … 399** is the whole usable band.

### 4.1 The fill rule (where the shipped data puts every edge)

The shipped pages answer every layout question the same way, and none of them stretches or
centres a row to fill space:

* **The content band is top-anchored at the tab row's bottom.** The tab row is
  `y 55 … 87` and the first section header of all four settings pages is at `y 87`
  (`menu_video` 157/87, `menu_advancedvideo` 34/87 and 251/87, `menu_audio` 157/87,
  `menu_gameoption` 34/87 and 359/87). Nothing floats; there is no centred page anywhere.
* **The content band is x 34 … 606, with a 4-unit pad before the first label.** The widest
  shipped content is `menu_gameoption` (`34 103 320 104` and `359 103 247 104` → 34…606) and
  `menu_advancedvideo` (`34 …` and `251 103 349 144` → 34…600); the first label inside each
  panel sits at x 38. The LCARS art's interior runs ~17…622, so that band is retail's own
  17-unit margin on each side.
* **A panel box is set by what follows it, never by its own rows.** A box ends 4 units above
  the next section header (`menu_advancedvideo` box `103…247`, next header 251) and the last
  section's box ends 4 under its tallest column (`menu_advancedvideo` §1 right: rows
  107…243, box `103…247`). Side-by-side panels share one bottom edge and the taller column
  sets it (`menu_advancedvideo` §1: both panels `103…247`, the right one's 7 rows against the
  left one's 5; `menu_gameoption` §1/§3 both `103…207`).
* **The shipped dead space is authored.** A short column leaves its tail inside the box:
  `menu_video` §2's box runs to 333 over a last row at 301, `menu_audio`'s single box runs to
  303 over a last row at 223. Retail never tightens the pitch to close it.
* **The pitch never varies within a page.** Every settings page is 20/16 end to end.

This port's rebuild follows all five: content x 34…606 (`s_videoCol`,
`uimenu.cpp:1699`), first header at 87 (`SET_PAGE_Y`), section = header 14 + gap 4, box
hugging its rows with the shipped 4-unit tail, and a **pitch of 26 with a row height of 26** —
the pitch is set to the row height, because eighteen rows in the 87…399 band leave room for
exactly 9 × 26 and the shipped ratio (row 16 of a 20 band) would otherwise put 6 loose units
inside every band. The page bottoms out at 395, one unit off `menu_advancedvideo`'s own 396,
against the description bar at 403.

**Do not re-derive the page's bottom by eye.** The band is
`SET_PAGE_Y + 3 × (14 + 4 + 3 × pitch + 4) + 2 × 4` = 395 at pitch 26 (three headers, three
boxes, two inter-section gaps); if a row is added, the pitch is what gives, not the box.

---

## 5. Titles, `$$token$$`, and the two ways text resolves

`title "$$FullScreen$$"` is stored resolved: `setTitle` runs `getStringResource` if the text
*is* a token (`uiwidget.cpp:3475-3481`), so `getTitle()` hands back the translated string, not
the token. Two consequences:

* a keep/hide test written in tokens will not match what `getTitle()` returns. Test **both**
  (the port's `SettingsVideoKeeps`, `uimenu.cpp:2109`, compares the title *and* the token's
  resolution).
* a token that resolves to nothing leaves the title **empty**, and an empty title must never
  match a keep rule — otherwise every titleless background panel is "kept" by accident.

`UILabel::Draw` re-converts at draw time when the string still carries `$$`
(`uilabel.cpp:257-262`), which is how a `linkstring`/`option` label can be authored as a token.

---

## 6. How a row becomes a cvar write

* `SwitchBox`: the option's own `<command>` is stuffed when the box cycles
  (`UISwitchBox::Pressed`, `uiswitchbox.cpp:299`). Retail writes `set <cvar> <value>`, which is
  why every `option` carries a full command line rather than a value.
* `CheckBox`: `Released` flips the mark, stuffs `checked_command`/`unchecked_command`, then
  `UpdateCvar` writes the linked cvar directly (`uicheckbox.cpp:187-203`).
* `Slider`: writes the cvar on every move, at `stepsize` granularity.
* `PulldownMenuContainer`: the popup's `addpopup` commands, same shape as `option`.

Whether a write needs a renderer restart is a property of the **cvar**, not the widget:
`CVAR_LATCH` cvars (`r_picmip`, `r_subdivisions`, `r_vertexLight`, `r_fullscreen` — all
registered with `CVAR_LATCH`, e.g. `vk_platform.c:439`) only take effect when the renderer
comes down, and `vid_restart` is the only thing that brings it down
(`CL_Vid_Restart_f`, `cl_main.cpp:1003` → `CL_ShutdownRef`/`CL_InitRef` → `R_SetMode` again,
which is what recreates the SDL window with the new `r_fullscreen`,
`vk_platform.c:456`/`:492`/`369-371`). Retail's own Apply button ran `ui_checkrestart`
(`menu_video.urc:208`); this port's shape is the auto-apply (`uimenu.cpp:754`): every latched
row records what its cvar read when the page was built (`SettingsBaselineRecord`,
`uimenu.cpp:722`), a row write pokes `ui_settingsautoapply`, and a settle window later the
restart lands once (`UI_SettingsAutoApplyFrame`, `uimenu.cpp:785`). Cycling a row back to its
baseline costs no restart, which is the whole point of recording one.

---

## 7. Hide, grey, delete: the three states a row can be in

| state | mechanism | the pad | touch | when to use it |
|---|---|---|---|---|
| **hidden** | `UIWidget::Disable()` → `ProcessEvent(EV_Widget_Hide)` → `setShow(false)` (`uiwidget.cpp:2897`) | not a stop (`moveFocus` requires `isEnabled() && getShow()`, `uiwidget.cpp:5017`) | not hit (`FindResponder` requires `m_visible`, `uiwidget.cpp:2928`) | the row does not exist on this target at all |
| **greyed** | `widgetstate disable` (`uiwidget.cpp:797`, `:2110`) — `WIDGET_STATE_DISABLED` is sticky against hover/armed (`setHoverState`, `uiwidget.cpp:2167`) plus `AllowActivate(false)` | not a stop (`CanActivate()` reads `m_canactivate`, `uiwidget.cpp:4205`) | drawn but inert: `UISwitchBox::Pressed` refuses the state (`uiswitchbox.cpp:301`), `UICheckBox::Released` returns (`uicheckbox.cpp:189`) | the row exists but this target cannot honour it |
| **present** | — | a stop | active | the normal case |

Greyed is what retail's own data uses for a row that must be seen but not used
(`disabledfgcolor`/`disabledshadercolor` ship in the settings pages, 6 uses each), and it is
EF1's answer too: `s_video_fullscreen_list.generic.flags |= QMF_GRAYED`
(`VoyagerNX_Android .../efgame/src/ui/ui_video.cpp:410`). **Hiding is not greying** — a hidden
row leaves a hole in the grid and I cannot see that the setting exists.

The grey set is **per target in both directions** (`SettingsRowIsGreyed`,
`uimenu.cpp:1758`): the handheld greys `r_fullscreen` (its surface is the whole window, so
the choice has nothing to change) and the desktop greys `android_aspect` (its window is
already the video mode's own shape, and the rect consumer ignores the cvar there — see
section 2). Each target's list is `s_videoDesktopRows` / `s_videoAndroidRows`; a row goes on
the list of the target that cannot honour it, and the row is still built on both, so the
page keeps its shape. One row, one list, and the row's state is the only thing that is per
target.

Two mechanisms that come with that, both paid for:

* **The consumer is gated with the row.** A greyed row is inert but its cvar is still
  archived, so a value saved on the other target must not keep working here —
  `SCR_AndroidDisplayRect`/`CG_AndroidDisplayRect` return the whole surface off-Android
  (`cl_scrn.cpp:61`, `cg_view.c:41`), which is also what makes the desktop's bars and mouse
  mapping bit-identical to a build without the feature.
* **The walk that greys must initialise its finds.** The grey loop reads `lbl` in its own
  `for` condition before the body writes it (`for (c = 0; c < 2 && !lbl; c++)`), so an
  uninitialised slot is a non-NULL "widget" that skips the walk and is then handed to
  `SettingsGreyRow`. That is how the 0.8.4 Android boot died in `Listener::ValidEvent` with
  `this` = the rodata literal `"tooltip_video"` (`uimenu.cpp:2283` materialises it two lines
  above the loop): `UIWidget::FindWidget` (`uiwidget.cpp:4371`) can only return a live child
  or `NULL`, so the literal came from the stack, not the child list. Both compilers flag the
  shape (`-Wconditional-uninitialized` / `-Wmaybe-uninitialized` on the reduced loop); the
  full function is too tangled for either, which is why the gate missed it. Declare the
  finds `= NULL`.

---

## 8. The port's own layer: adapt, keep, build

`UI_AdaptSettingsMenus` (`uimenu.cpp:2498`) walks every top-level container once per UI init
and, per page:

1. `AdaptHideRow` — the static per-page hide tables (`s_adaptRows`, `s_adaptRowsAndroid`,
   `uimenu.cpp:1551`): rows a renderer never reads, hidden by cvar name, widget name or
   shader name.
2. `SettingsAdaptVideo` (`uimenu.cpp:2277`) — the graphics page is *rebuilt*: retail's own rows
   are hidden by a keep list (`SettingsHideVideoRetail`, `uimenu.cpp:2142`), then the port's
   own rows are created from one declarative table (`s_videoRows`, `uimenu.cpp:2177`) by
   `SettingsBuildRows` (`uimenu.cpp:1941`).

`SettingsBuildRows` is the layout model, and it is deliberately one model:

* the table is walked once; a `section` entry opens a full-width header plus a panel box and
  resets the row counter; each row takes the next slot — left, right, left, right — one band
  per two rows (`uimenu.cpp:1997`);
* each built row **remembers its slot** (`rowy[]`, `rowcol[]`, `uimenu.cpp:2029`), so the
  second half of the pass re-issues the final rectangles from the recorded slot instead of
  recounting the table. The original code recounted it and dropped a slot at every section
  boundary: every row after a section header was re-issued one slot early, which walked
  Geometry Detail into the Lighting band (under that band's panel, so it drew dimmed), walked
  Shadows and Flares into the Effects band, and buried Effect Detail and Deform Geometry under
  Detail Surfaces and Detail Textures at the same rectangle. **A pass that lays out rows must
  not walk the row table twice with two counters.**
* the label column is one width for the whole page, taken from the longest label the page
  carries, measured in authored units (`SettingsTitleWidth`, `uimenu.cpp:1784`); the value
  column keeps its right edge and gives up the difference. One label width, one value width,
  per column pair, is what makes the columns line up across bands.
* a page builds once. The guard must test a widget that is actually created
  (`VidLHead1`, the first section header, `uimenu.cpp:2286`) — the previous guard tested a name
  nothing was ever given, so every UI re-init (every `vid_restart`) built the page again on top
  of itself.

Everything this layer creates is named `<prefix>_<cvar>` and `<prefix>_<cvar>_label`, so a
`globalwidgetcommand` aimed at it cannot land on another page (section 3).

---

## 9. Probes

All behind `qa_probe` (`uimenu.cpp:31`, the `qa_probe.h` include). The rows a boot should
show:

| row | meaning |
|---|---|
| `[UIVK:settings] op=hide menu='video' hidden=N art=N` | the shipped rows going away, and the LCARS art kept |
| `[UIVK:settings] op=build menu='video' rows=N bottom=M` | the rebuild, and where its content ends (the description bar is at 403) |
| `[UIVK:settings] op=labelw authored=A measured=M` | the label column was widened past the authored guess |
| `[UIVK:settings] op=row cvar=<cvar> state=live\|greyed target=desktop\|android` | each per-target row's state on this target: `r_fullscreen` is greyed on Android, `android_aspect` on the desktop, and the other one prints `live` |
| `[UIVK:settings] op=autoapply cvar=<cvar> old=A new=N` then `op=autoapply restart=vid_restart` | a latched row moved and the renderer is coming down once |
| `[UIVK:padnav] to=<name>` | each row the pad walked onto |
| `[UI:switchopt] cvar= cmd= label=` | every SwitchBox selection |

---

## 10. Recipe: add one setting row

The short path, on the graphics page (any other page: same steps, different table).

1. **Name the cvar and prove it is read.** Grep the renderer for it. If nothing reads it, the
   row is a lie — that is how `r_texturebits`, `r_texturemode`, `r_allowExtensions`,
   `r_arb_multisample`, `r_ext_nv_multisample_filter` and `r_anisotropicFiltering` were all
   cut (DESIGN.md 2.2 carries the proof per cvar). Note whether it is `CVAR_LATCH`.
2. **Add the row to `s_videoRows`** (`uimenu.cpp:2177`). A SwitchBox needs one
   `{"<label>", "set <cvar> <value>", "<value>"}` triple per choice; a CheckBox needs nothing
   but the label; a Slider needs `lo, hi, step, type`. Set `latched = 1` when the cvar is
   `CVAR_LATCH` — that is what attaches the auto-apply restart.
3. **Keep the bands square.** The page is two columns; a section with an odd number of rows
   leaves its last right cell empty. Put the new row where the section stays even, or accept
   the hole on purpose.
4. **If the row is per-target**, add its cvar to the list of the target that *cannot* honour
   it — `s_videoDesktopRows` (greyed on the handheld) or `s_videoAndroidRows` (greyed on the
   desktop), both next to `SettingsRowIsGreyed` (`uimenu.cpp:1738`). It is then built on both
   targets and greyed on one — not hidden, so the page keeps its shape — and if the row's
   cvar changes geometry, gate the consumer the same way (`SCR_AndroidDisplayRect`,
   `cg_view.c`), because a greyed row's cvar is still archived. Look the row's widgets up
   through the prefix of the column it actually landed in (`SettingsAdaptVideo`,
   `uimenu.cpp:2309`): the widget name carries the column prefix, so a row that landed right
   is `VidR_<cvar>`, not `VidL_<cvar>`.
5. **Never invent a cvar.** Every row drives one the engine already registers, so the
   launcher's per-boot override chain (`personal/scripts/play_ef2_decomp.sh`) and
   `defaultoptions.cfg` cannot re-pin it out from under its own toggle.
6. **Gate**: `ninja` on the Linux reference and the Android NDK target, `ldd -r` clean on every
   touched `.so`, then one armed boot on the pad: open Settings → Video, walk the page
   (every row names itself in `[UIVK:padnav]`), press ENTER on the new row, and read the
   `op=row` / `op=autoapply` rows.

The same recipe on a *shipped* page this port does not rebuild (audio, GameOptions): add the
row to that page's `.urc` instead, with the page's own label/value x values from the table in
section 4, and hide or retitle nothing you cannot prove.
