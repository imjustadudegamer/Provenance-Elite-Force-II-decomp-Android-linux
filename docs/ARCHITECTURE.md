# Architecture

A native Android port of *Star Trek: Voyager, Elite Force II*, built on a
de-MOHAAd id Tech 3 engine lineage. Every function, struct, table and constant is
held to the EF2 SDK and the retail Mac PPC binaries; the engine, the game DLL,
the cgame, the GLES 3.0 renderer and the tiki/skeletor model layer all build from
one tree.

## Repository layout

| Path | Contents |
|---|---|
| `app/` | the Android module (`build.gradle.kts`, manifest, assets, java, res) |
| `app/src/main/cpp/` | **all native source**: the engine modules, the renderer, the vendored third-party libraries, the CMake build |
| `docs/` | project documentation (this file, `BUILD.md`, `RENDERER.md`, `PROVENANCE.md`, `ANDROID_PORT.md`) and `docs/closeouts/` for historical material |
| `personal/scripts/` (untracked) | the build/capture wrappers |
| `tools/` | release tooling: the notice codemod, `license_lint.py`, `ship_text_lint.py`, `provenance_ledger.py` |
| `LICENSING.md`, `PROVENANCE.tsv`, `COPYING.txt` | the licence statement, the per-file provenance ledger and the GPLv2 text. `docs/EF2-SDK-licenseAgreement.txt` is the Ritual SDK agreement, shipped verbatim |
| `personal/evidence/`, `personal/oracle/` | untracked, on-disk-only material: the Mac PPC decompile corpus, the retail binary, the capture corpora |

## Licensing

The tree carries three bodies of source under different terms: the id Tech 3 /
OpenMoHAA engine lineage (GPLv2 or later, `COPYING.txt`), the Elite Force II game
layer derived from the Ritual SDK (the limited-use agreement at
`docs/EF2-SDK-licenseAgreement.txt`), and material rebuilt from the retail
binaries, over which the port asserts no copyright. The GPL and the SDK terms
cannot cover one file, so no single licence covers the tree; every source file
carries the notice for its own provenance, `PROVENANCE.tsv` records it, and
`tools/license_lint.py` enforces it. See `LICENSING.md`.

## Native modules (`app/src/main/cpp/code/`)

| Module | Role |
|---|---|
| `qcommon/` | shared core: filesystem, cvars, commands, netcode, common frame, the console |
| `client/` | the client: connection state machine, input, sound, UI host, save manager, renderer interface |
| `server/` | the server: client slots, snapshots, game import, networking |
| `fgame/` | the game module, entities, actors, weapons, AI, script responses |
| `cgame/` | the client-game module, prediction, entities, HUD, radar, effects |
| `uilib/` | the menu library: widgets, menus, the UI import/export tables |
| `renderergles3/` | the shipping renderer: GLES 3.0 programs, state, buffers, the shader model |
| `renderercommon/` | renderer headers shared across backends |
| `tiki/`, `skeletor/` | the model container/parser and the skeletal animation layer |
| `botlib/` | the bot AI library (AAS routing and navigation) |
| `script/` | the script engine and string resources |
| `corepp/` | the port's container/string/memory layer |
| `sdl/`, `sys/` | platform glue: window/event/context and OS services |
| `gamespy/`, `null/`, `web/`, `tools/` | vendored service glue, null stubs, the updater, build tools |
| `thirdparty/` | zlib, jpeg, libmad, OpenAL Soft, SDL2, fmt and the other vendored libraries |

## Module boundaries

`game.so` and `cgame.so` are dlopened at run time and talk to the engine through
`game_import_t` / `cgi` function tables, exactly as retail does. `uilib` is
compiled into the same executable as the client and reaches the engine through
`uiimport_t` and `uiexport_t`. The renderer is a separate shared object behind
`refexport_t`, selected by the `cl_renderer` cvar.

## Platform and renderer switch

Two CMake cache variables select the target:

- `EF2_PLATFORM`, `linux` or `android`; picks the platform glue and the data
  root.
- `EF2_RENDERER`, `gles3`; the fixed-function and Vulkan backends are deleted,
  so `gles3` is the only value.

The accepted divergences from retail are confined to platform glue, the sound
backend and the GLES 3.0 renderer/platform layer. Everything else is held
retail-exact against the SDK and the retail binaries; the citations that back
each claim are recorded in `personal/RE_REFERENCE_CITATIONS.tsv`.
