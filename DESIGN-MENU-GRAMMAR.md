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
`UI_AdaptSettingsMenus`'s page test (`uimenu.cpp:2581`, `stricmp(container->getName(), "video")`).

`background.inc` ships six `Widget` resources (`main_bkg_01` … `main_bkg_06`) and the `logo`,
each with a **name and no title** (`personal/evidence/urc_retail2/background.inc`). That is
the "titleless widget" convention: a pure paint resource. It matters twice — it draws before
everything after it in the file, and any pass that keeps pages by *title* will drop it
(which is what the first round of this lane did; `s_videoKeepNames` in `uimenu.cpp:2110`
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
overlap. `SettingsTitleWidth` (`uimenu.cpp:1812`) is the port's one conversion point: measure,
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

| page | label | value | label→value | arrow | pitch |
|---|---|---|---|---|---|
| `menu_video` | x 162 w 156 | x 322 w 140 | **+4** | x 462 w 16 | 20 (y 108, 128, 148, 168, 188) |
| `menu_audio` | x 162 w 156 | x 322 w 156 (slider) or 140 (switch) | **+4** | x 462 w 16 | 20 (y 107…207) |
| `menu_advancedvideo` | x 38 w 185 / x 255 w 185 | x 223 w 140 / x 440 w 140 | **0** | x 580 w 16 | 20 |
| `menu_gameoption` | x 38 w 156 / x 362 w 120 | x 194 w 156 / x 482 w 104 | **0** | x 586 w 16 | 20 |
| `menu_controls` | four `Table`s at `46 124 325 255`, one per tab | — | — | — | the table's `itemheight` |

Five things to copy from those numbers:

* **The label box and the value box are one plate.** The value box's left edge sits on the
  label box's own right edge on the two-column pages (`menu_advancedvideo` 38 + 185 = 223
  with its value at 223, `menu_gameoption` 38 + 156 = 194 with its value at 194) and 4 past
  it on the one-column pages (`menu_video`, `menu_audio`: 162 + 156 = 318, value at 322).
  No shipped page puts a strip of page background between the two, and none overlaps the
  pair — the two numbers above are the whole shipped range. This port's rebuild is the
  two-column shape and carries `menu_advancedvideo`'s rows, so it takes the edge-to-edge rule
  (`SET_LABEL_GAP 0`, `uimenu.cpp:1758`); a value box that opens 4 past the label plate is
  the one-column pages' slack, not a rule.
* **The columns are 12 apart, not edge to edge.** In both shipped two-column pages the left
  column's own control — the value box plus its 16-wide pulldown arrow where it has one —
  ends 12 units before the right column's first label plate: `menu_gameoption` value
  194…350 against label 362, `menu_advancedvideo` §3 value+arrow 223…379 against label 391.
  Retail never stretches a value box across the seam, and the seam is the same 12 in both
  pages, so this rebuild pins its left value box's right edge with it
  (`s_videoCol[0].valx + valw` = 315 against the right label at 327, `uimenu.cpp:1730`).
* **20 is the shipped row pitch, 16 is the shipped row height.** A page that needs more rows
  than fit uses a taller pitch and taller rows (this port's rebuild derives its own from the
  band it has to fill — see the fill rule below — to fit eighteen rows in the same space,
  `uimenu.cpp:1727`).
* **a section is a pair of widgets: a `Label` header with a background colour, and a `Widget`
  box behind its rows.** `menu_video.urc:11-35` (`VideoBackground`, `rect 157 87 326 16` then
  `rect 157 103 326 105`); the header is 16 tall and **the box starts on the header's own
  bottom edge** (87 + 16 = 103). `menu_advancedvideo.urc` uses three such pairs, two side by
  side (`rect 34 87 215 16` / `rect 34 103 214 144`). All four pages do it: header 87…103,
  box 103…, so the header and its panel read as one block and no page background shows
  between them.
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
* **A header is 16 tall and its panel starts on the header's own bottom edge.** All four
  pages: header `87…103`, box `103…`. The gap is zero, not 4 — the header is the panel's
  own cap, and what separates two sections is the 4 units between one panel's bottom and
  the next header's top (`menu_video` box 103…208, next header 212; `menu_gameoption`
  box 103…207, next header 211; `menu_advancedvideo` box 103…247, next header 251).

This port's rebuild follows all of them: content x 34…606 (`s_videoCol`,
`uimenu.cpp:1730`), first header at 87 (`SET_PAGE_Y`), **header 16 tall with the panel on
its own bottom edge** (`SET_HEAD_H 16`, `SET_HEAD_GAP 0`, `uimenu.cpp:1699`), the box
hugging its rows with the shipped 4-unit tail, and the columns 12 apart. The pitch is the
one number retail does not hand over, because retail's sections carry 4-7 rows and this
rebuild carries 6 a band at a pitch that would run the page past the description bar, so
the band hands out what is left after the shipped numbers:

```
pitch = rowh = (395 − 87 − 3×16 − 3×4 − 2×4) / 9 = 240 / 9 = 26.67   (SET_VIDEO_PITCH, uimenu.cpp:1727)
```

Three headers of 16, three box tails of 4, two inter-section gaps of 4, and nine bands take
the rest. The page bottoms out at 395, one unit off `menu_advancedvideo`'s own 396, against
the description bar at 403 — and the only loose unit left below the last row is retail's own:
4 inside the box, then 8 of page background to the bar, against retail's 4 and 7.

**Do not re-derive the page's bottom by eye.** The band is
`SET_PAGE_Y + 3 × (16 + 0 + 3 × pitch + 4) + 2 × 4` = 395 at pitch 26.67 (three headers, three
boxes, two inter-section gaps); if a row is added, the pitch is what gives, not the box.

**The handheld's touch page follows the same rules with one band of its own.** It has no
shipped page to copy (`menu_controls.urc` ships bind tables, not label+value rows), so the
band it fills is the shipped controls page's own content area: x 46…371 — the shipped tabs run
`46 87 162 16` … `209 104 163 16`, so 46…372, and the page's right column opens at 374 — with
the same 4-unit pad before the first label (`s_touchCol`, `uimenu.cpp:1742`). Its one header
opens on its own tab row's bottom, `SET_TAB_Y + SET_TAB_H` = 103, which is the same
top-anchor rule the shipped pages follow with their own tab row, and it is 16 tall with the
panel on its bottom edge like theirs. The six rows then take the whole band:
**pitch = rowh = (395 − 103 − 16 − 4) / 6 = 272 / 6 = 45.33** (`SET_TOUCH_PITCH`,
`uimenu.cpp:1739`), six rows a page, one section — so its box ends on the video page's own
bottom line, 395, eight above the description bar at 403. That is the one rule this page adds
to the others: *the pitch is the band divided by the rows the page actually carries*, which is
why its rows are 45.33 where the video page's are 26.67 and both pages end on the same line.
Six 45-unit rows are a finger target by construction — the row *is* the tap area, and the
checkbox mark stays its own 16 × checkScale size (`uicheckbox.cpp:116-117`) centred in it.

---

## 5. Titles, `$$token$$`, and the two ways text resolves

`title "$$FullScreen$$"` is stored resolved: `setTitle` runs `getStringResource` if the text
*is* a token (`uiwidget.cpp:3475-3481`), so `getTitle()` hands back the translated string, not
the token. Two consequences:

* a keep/hide test written in tokens will not match what `getTitle()` returns. Test **both**
  (the port's `SettingsVideoKeeps`, `uimenu.cpp:2137`, compares the title *and* the token's
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
(`menu_video.urc:208`); this port's shape is the auto-apply (`uimenu.cpp:757`): every latched
row records what its cvar read when the page was built (`SettingsBaselineRecord`,
`uimenu.cpp:722`), a row write pokes `ui_settingsautoapply`, and a settle window later the
restart lands once (`UI_SettingsAutoApplyFrame`, `uimenu.cpp:785`). Cycling a row back to its
baseline costs no restart, which is the whole point of recording one.

### 6.0 The page never writes a cvar it did not read from me

A settings page snapshots its rows' cvars when it is pushed and writes them back when it is
popped with a restore: `Menu::SaveCVars` (`uimenu.cpp:446`) copies every linked cvar's string
into its widget, `Menu::RestoreCVars` (`uimenu.cpp:470`) sets them back, and
`MenuManager::PopMenu` (`uimenu.cpp:1278`) calls the restore only when its argument says so.
Retail's own pages all leave through `popmenu 0` (`menu_video.urc`, `menu_audio.urc`,
`menu_gameoption.urc`, `menu_controls.urc`, `menu_advancedvideo.urc` — every Back/Main Menu
button), and retail's own ESC does the same: `UI_MenuEscape @0x00194A00` calls
`PopMenu(this, 0)`. This port's ESC arm popped 1, so leaving the settings screen rewrote
every row with what it had read at open — the toggles went with it, and a row whose boot
value came from the shipped data went back to the shipped data's value. That is the whole
"my toggles do not hold" shape, and it is one flag: `PopMenu(qfalse)`
(`cl_ui.cpp:1728`), retail's own argument.

Two more writers live in the widget classes and both are retail's own, so they are recorded
here rather than changed:

* `UISlider::UpdateData` (`uislider.cpp:292`) writes its linked cvar, and it runs on press,
  release, arrow step and drag — never on build or draw, because `UpdateUIElement`
  (`uislider.cpp:331`) is the read side. A slider therefore writes only when I move it, and
  the write formats the value (`UI_SetCvarFloat`), which is why an untouched `r_gamma`
  resaves as `1.2` rather than `1.200000`.
* `UICheckBox::Draw` (`uicheckbox.cpp:139`) calls `UpdateCvar` while the widget is
  **disabled** — and `UpdateCvar` (`uicheckbox.cpp:214`) forces `m_checked` to 0 first, so a
  greyed checkbox zeroes its own cvar every frame it draws. Retail's `UICheckBox::Draw`
  @0x00054E70 does exactly the same, so it stays; the rule it imposes is never to grey a
  CheckBox row. Both greyed rows in this port are SwitchBoxes, whose `Draw`
  (`uiswitchbox.cpp:349`) only reads.

### 6.1 A row's options and a row's default are both shipped facts

The option list is the shipped page's, not the row author's. Retail's own Shadows and Flares
rows (`menu_advancedvideo.urc:447-449` and `:527-529`) carry three options each, in this
order, at these values, and a SwitchBox can only show a value it has an option for
(`UISwitchBox::Draw` leaves the box on whatever it was last when the cvar matches nothing,
`uiswitchbox.cpp:361-403`):

| row | cvar | the shipped options |
|---|---|---|
| Shadows | `cg_shadows` | `$$None$$` 0, `$$Simple$$` 1, `$$Stencil$$` 2 |
| Flares | `r_flares` | `$$None$$` 0, `$$Simple$$` 1, `$$Precise$$` 2 |

A cvar sitting on a value the row does not list is a row that lies: this port's Flares row
carried None/On only while the defaults pin `r_flares 2`, so a fresh install showed a box
that could not name its own value. The list comes out of the shipped page, whole, in order.

The default is a registration fact, and it can lose to the shipped data. `global/game.cfg`
ships inside `pak0.pk3`, is exec'd once at boot before the config (`common.c:1554`), and its
`setd` lines rewrite **both the value and the reset string** of a cvar that does not exist
yet (`Cvar_SetD_f`, `cvar.c:831`; retail's own is the same shape at `0xCD100` in the Mac
engine). That runs before the renderer loads, so a renderer registration cannot answer it:
the port's `r_dynamiclight "1"` (`tr_init.c:216`) arrives after `setd r_dynamiclight 0` and
`Cvar_Get` keeps the earlier default (`defaultRecorded`, `cvar.c:368`). On a fresh config
the row therefore opened at 0 — dynamic lights off — with the registration default never
read.

Two facts together give the registration its answer back. First, `defaultRecorded` is the
only marker a pak `setd` leaves, and it means exactly "this reset string came from data, not
from code". Second, a value out of config.cfg is not a default at all — it is mine — so the
config exec marks itself and `Cvar_Set2` clears the marker on everything it writes while it
is set (`cvar_inConfig`, `common.c:1626`, `cvar.c:44`, `cvar.c:506`). What is left carrying
the marker after the config has had its say is a cvar the shipped data defaulted and nothing
else has ever touched, and that is the one case where the code default owns the value:
`R_RegisterLightCvar` (`tr_init.c:171` in the Vulkan renderer, `:1124` in the GLES 3.0 twin)
applies it for the two rows this page carries. So:

* a fresh config opens at `r_dynamiclight 1` and `r_flares 2` (Precise — the unified
  defaults' own pin, `ef2_android_defaults.h:221`), both on;
* a value I archived — either row, either value — clears the marker during the config exec
  and nothing touches it afterwards;
* the desktop launcher's `setx r_flares 2` / `setx r_dynamiclight 1`
  (`personal/scripts/play_ef2_decomp.sh:328`) pins neither to 0 and lands before the
  registration either way.

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
`uimenu.cpp:1786`): the handheld greys `r_fullscreen` (its surface is the whole window, so
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
  `this` = the rodata literal `"tooltip_video"` (`uimenu.cpp:2315` materialises it two lines
  above the loop): `UIWidget::FindWidget` (`uiwidget.cpp:4371`) can only return a live child
  or `NULL`, so the literal came from the stack, not the child list. Both compilers flag the
  shape (`-Wconditional-uninitialized` / `-Wmaybe-uninitialized` on the reduced loop); the
  full function is too tangled for either, which is why the gate missed it. Declare the
  finds `= NULL`.

---

## 8. The port's own layer: adapt, keep, build

`UI_AdaptSettingsMenus` (`uimenu.cpp:2538`) walks every top-level container once per UI init
and, per page:

1. `AdaptHideRow` — the static per-page hide tables (`s_adaptRows`, `s_adaptRowsAndroid`,
   `uimenu.cpp:1551`): rows a renderer never reads, hidden by cvar name, widget name or
   shader name.
2. `SettingsAdaptVideo` (`uimenu.cpp:2309`) — the graphics page is *rebuilt*: retail's own rows
   are hidden by a keep list (`SettingsHideVideoRetail`, `uimenu.cpp:2170`), then the port's
   own rows are created from one declarative table (`s_videoRows`, `uimenu.cpp:2205`) by
   `SettingsBuildRows` (`uimenu.cpp:1969`).
3. `SettingsAdaptControls` (`uimenu.cpp:2435`, handheld only) — the controls page keeps its
   four shipped bind tables and adds the fifth tab plus the touch rows, through the same
   `SettingsBuildRows` and `s_touchRows`. A page built this way is *not* rebuilt on tab
   switch: the tab command only enables its own rows and disables the rest
   (`SettingsTabCommand`), and the header, the box and every row are created hidden and left
   to that command — which is why they are disabled right after the build, since the tab
   commands all ran *before* the widgets existed.

`SettingsBuildRows` is the layout model, and it is deliberately one model:

* the table is walked once; a `section` entry opens a full-width header plus a panel box and
  resets the row counter; each row takes the next slot — left, right, left, right — one band
  per two rows (`uimenu.cpp:2026`);
* each built row **remembers its slot** (`rowy[]`, `rowcol[]`, `uimenu.cpp:2057`), so the
  second half of the pass re-issues the final rectangles from the recorded slot instead of
  recounting the table. The original code recounted it and dropped a slot at every section
  boundary: every row after a section header was re-issued one slot early, which walked
  Geometry Detail into the Lighting band (under that band's panel, so it drew dimmed), walked
  Shadows and Flares into the Effects band, and buried Effect Detail and Deform Geometry under
  Detail Surfaces and Detail Textures at the same rectangle. **A pass that lays out rows must
  not walk the row table twice with two counters.**
* the label column is one width for the whole page, taken from the longest label the page
  carries, measured in authored units (`SettingsTitleWidth`, `uimenu.cpp:1812`); the value
  column keeps its right edge and gives up the difference. One label width, one value width,
  per column pair, is what makes the columns line up across bands — and it is why the
  label→value offset is a constant: the value box opens on the label box's right edge
  (`SET_LABEL_GAP`, section 4), so the measured label width cannot open a slit.
* **every width the helper lays out comes from `cols[1]`** — the section header's and the
  panel box's width is `cols[1].boxx + cols[1].boxw − cols[0].boxx`, whatever `ncols` says,
  because a one-column page is two names for one column. A one-column page must therefore
  hand it the same column twice (`const settingsGeom_s cols[2] = {s_touchCol, s_touchCol}`);
  handing it the one-element array reads past the array, and the header and the box take
  their width from whatever the linker put next. On the touch page that was the whole
  defect's second half: the six rows drew and neither the header nor the box did, so the
  page read as six floating plates over a hole. **A caller's array must have the elements
  the callee's contract names, not the ones this call happens to read.**
* a page builds once. The guard must test a widget that is actually created
  (`VidLHead1`, the first section header, `uimenu.cpp:2318`) — the previous guard tested a name
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
| `[UI:cvarsave] menu= <cvar>='<v>'` | what a page snapshotted at push — one row per linked cvar |
| `[UI:cvarrestore] menu= <cvar>='<v>'` | a page writing its snapshot back. **Leaving a settings page must print none of these**; the only arm left that restores is the console's own `popmenu 1` |

---

## 10. Recipe: add one setting row

The short path, on the graphics page (any other page: same steps, different table).

1. **Name the cvar and prove it is read.** Grep the renderer for it. If nothing reads it, the
   row is a lie — that is how `r_texturebits`, `r_texturemode`, `r_allowExtensions`,
   `r_arb_multisample`, `r_ext_nv_multisample_filter` and `r_anisotropicFiltering` were all
   cut (DESIGN.md 2.2 carries the proof per cvar). Note whether it is `CVAR_LATCH`.
2. **Add the row to `s_videoRows`** (`uimenu.cpp:2205`). A SwitchBox needs one
   `{"<label>", "set <cvar> <value>", "<value>"}` triple per choice; a CheckBox needs nothing
   but the label; a Slider needs `lo, hi, step, type`. Set `latched = 1` when the cvar is
   `CVAR_LATCH` — that is what attaches the auto-apply restart.
3. **Keep the bands square.** The page is two columns; a section with an odd number of rows
   leaves its last right cell empty. Put the new row where the section stays even, or accept
   the hole on purpose.
4. **If the row is per-target**, add its cvar to the list of the target that *cannot* honour
   it — `s_videoDesktopRows` (greyed on the handheld) or `s_videoAndroidRows` (greyed on the
   desktop), both next to `SettingsRowIsGreyed` (`uimenu.cpp:1766`). It is then built on both
   targets and greyed on one — not hidden, so the page keeps its shape — and if the row's
   cvar changes geometry, gate the consumer the same way (`SCR_AndroidDisplayRect`,
   `cg_view.c`), because a greyed row's cvar is still archived. Look the row's widgets up
   through the prefix of the column it actually landed in (`SettingsAdaptVideo`,
   `uimenu.cpp:2341`): the widget name carries the column prefix, so a row that landed right
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
