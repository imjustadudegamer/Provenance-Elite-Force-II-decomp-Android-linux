# ProvenanceEF2 — Star Trek: Voyager — Elite Force II (Android)

A native Android port of *Star Trek: Voyager — Elite Force II*, built on a
de-MOHAAd id Tech 3 engine lineage. Every function, struct, table and constant is
held to the EF2 SDK and the retail Mac PPC binaries; the engine, the game DLL, the
cgame, the GLES 3.0 renderer and the tiki/skeletor model layer all build from the
tree in `app/src/main/cpp/`.

No game data is included. Build the APK; supply your own retail *Elite Force II*
installation at run time.

## Layout

| Path | Contents |
|---|---|
| `app/` | the Android module (`app/build.gradle.kts`, manifest, assets, java, res) |
| `app/src/main/cpp/` | **all native source**: engine, `fgame`, `cgame`, `renderergles3`, tiki, skeletor, uilib, qcommon, and the vendored third-party libraries |
| `docs/` | the provenance documentation (board, rules, reverse-engineering reference, validation, release plan) and `docs/closeouts/` for historical material |
| `docs/tools/`, `docs/scripts/` | evidence tools and the build/capture wrappers |
| `docs/evidence/` | untracked, on-disk-only evidence: the Mac PPC decompile corpus, the retail PC binary, the SDK source and the oracle corpus |

## Build

Prerequisites: Android SDK (`android-35`), NDK `27.0.12077973`, CMake `3.31.5`,
JDK 17+, Gradle 8.13 (the wrapper pins it). Point Gradle at the SDK with a
`local.properties`:

```sh
echo "sdk.dir=$HOME/Android/sdk" > local.properties
```

Build the APK:

```sh
app/build_apk.sh assembleDebug          # output: app/build/outputs/apk/debug/app-debug.apk
```

Build the Linux reference (the same engine, the same `EF2_PLATFORM`/`EF2_RENDERER`
switch):

```sh
cd app/src/main/cpp
cmake --preset linux-gles3 && ninja -C build-linux-gles3 -j16
```

Artifacts: `build-linux-gles3/Release/{provenanceef2,provenanceef2ded,game.so,cgame.so,renderer_gles3.so}`.
The gate for every change: 0 warnings / 0 errors on a clean rebuild and
`ldd -r` 0 undefined on all five artifacts.


## Documentation

Start at `docs/WORK_INDEX.md`. The binding rules are `docs/STANDING_RULES.md`;
the live board is `docs/DIVERGENCE_LEDGER_0904.md`; the reverse-engineering
reference is `docs/RE_REFERENCE.md` with the machine-readable citation index
`docs/notes/RE_REFERENCE_CITATIONS.tsv`.

## Licence

The engine lineage is GPL (see `app/src/main/cpp/COPYING.txt`); see `CREDITS.md`
and `THIRDPARTY.md`. This is an unofficial, non-commercial fan project, not
affiliated with or endorsed by any rights holder. No game assets are distributed.
