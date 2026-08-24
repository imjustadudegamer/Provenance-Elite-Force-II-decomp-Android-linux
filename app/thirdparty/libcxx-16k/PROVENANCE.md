# libc++_shared.so, 16 KB page-aligned

`arm64-v8a/libc++_shared.so` is the **unmodified** file from

    Android NDK 27.0.12077973
    toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so

copied byte for byte.  The build compiles the engine with **NDK r25.1.8937393**;
only this runtime file is taken from r27, because r25's `libc++_shared.so` is
4 KB page-segment aligned and Android 15's 16 KB page-size requirement rejects
it at load (that was the "16 KB" error: every engine module is 0x4000-aligned,
the NDK runtime shipped alongside it was 0x1000).

Why this is safe:

| check | result |
|---|---|
| LOAD segment alignment | `0x4000` (16384) on every LOAD |
| `DT_SONAME` | `libc++_shared.so` — identical to the r25 one it replaces |
| `DT_NEEDED` | `libc.so`, `libm.so`, `libdl.so` (a superset of r25's `libc.so`+`libdl.so`; both are public Android libs) |
| C++ runtime symbols our r25-built modules import | all 22 present (`__cxa_*`, `__gxx_personality_v0`, operator new/delete, `__ndk1::chrono::steady_clock::now`, the `__cxxabiv1` vtables, typeinfo for `char*`/`const char*`) — checked symbol for symbol against `llvm-nm -D` |
| mangling namespace | `__ndk1`, same as r25's |
| `BIND_NOW` | set |

The engine keeps building with r25: this file is only what ships in
`lib/arm64-v8a/`.  It is wired in by `app/build.gradle`'s
`fixLibcxxPageAlignment` task, which overwrites the copy
`merge<Variant>NativeLibs` produced (with the r25 one) before
`strip<Variant>DebugSymbols` runs, so nothing else in the APK is touched.

If the toolchain is ever moved to r27 wholesale, delete this directory and the
task: AGP then picks up the aligned runtime itself.
