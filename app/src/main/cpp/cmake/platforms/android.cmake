# Android (r133-android) -- the device target: arm64-v8a, the GLES 3.0
# backend, the engine built as libmain.so and driven by SDL2-for-Android.
#
# Everything here is the accepted-divergence platform layer (the operator's
# 0914 boundary: platform glue / sound backend / video backend are
# legitimately per-platform).  Nothing above the substrate moves: the same
# engine front-end, game module, cgame and tiki/skeletor/uilib sources build
# on both sides of the switch.
#
# Shape taken from the operator's shipped EF1 Android port
# (VoyagerNX_Android/android-port-Single-Player/EFAndroid-SP), which solved
# these problems on-device:
#
#   * the engine is ONE shared library (libmain.so) whose main() is renamed
#     SDL_main so SDLActivity's bootstrap thread can call it
#     (EF1 app/jni/src/CMakeLists.txt: "Rename sys_main.c's main() to
#     SDL_main so SDLActivity's bootstrap calls it");
#   * SDL2 is built in-tree as a shared lib and provides the window, the
#     event pump, the audio device and the ES 3.0 context
#     (SDL_CreateWindow(SDL_WINDOW_OPENGL) + SDL_GL_CreateContext);
#   * game data lives in the app's own external dir, which needs no storage
#     permission (ANDROID_DATA_ROOT, see code/sys/sys_unix.c);
#   * every .so is linked 16 KB-aligned so the APK also installs on the
#     Android 15+ 16 KB page-size devices (EF1 app/jni/CMakeLists.txt).
#
# The one deliberate difference from EF1: EF2 keeps retail's module boundary.
# Retail EF2 ships game.so / cgame.so beside the engine and dlopens them, and
# this tree already builds them as shared libraries with zero undefined
# symbols, so they are packaged as-is (libgame.so, libcgame.so,
# librenderer_gles3.so) instead of being statically folded into libmain.so.
# See code/sys/sys_main.c Sys_LoadDll for the loader side of that.

if(NOT EF2_PLATFORM STREQUAL "android")
    return()
endif()

# ---------------------------------------------------------------------------
# What the device target does not build.
#
# BUILD_SERVER       -- a dedicated server executable cannot be launched from
#                       an APK (no exec() of packaged files); single-player
#                       needs only the in-process server the client already
#                       carries.
# BUILD_GAME_QVMS    -- the q3lcc/q3asm toolchain is an ExternalProject built
#                       for the HOST and then run at build time; under the NDK
#                       toolchain it would compile for arm64 and never run.
#                       EF2 ships no QVM: retail's UI/game are the native
#                       modules this build packages (code/uilib is compiled
#                       into the client, fgame/cgame into their .so's).
# LTO                -- the NDK link of five large C++ shared libraries is
#                       where the extra minutes go; the script modules keep
#                       their own LTO-off rule (basegame.cmake) either way.
# ---------------------------------------------------------------------------
set(BUILD_SERVER OFF CACHE INTERNAL "Android: no dedicated server binary")
set(BUILD_GAME_QVMS OFF CACHE INTERNAL "Android: the qvm toolchain is a host tool")

if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
    message(STATUS "Android: LTO disabled (NDK link time)")
endif()

# ---------------------------------------------------------------------------
# 16 KB alignment.  Android 15+ devices may run with a 16 KB page size and
# refuse to load a 4 KB-aligned ELF segment.  NDK r25 defaults to 4 KB.
# (EF1 app/jni/CMakeLists.txt: "Align shared-lib segments to 16 KB so the app
# is compatible with Android 15+ 16 KB page-size devices".)
# ---------------------------------------------------------------------------
add_link_options(-Wl,-z,max-page-size=16384)

# Strip the absolute host path out of __FILE__ / assert strings / debug info so
# the developer's home directory never leaks into a shipped .so.  Same rule EF1
# applies, and the same rule renderer_common.cmake already applies to the
# renderers on every platform.  Applies to every target defined after this.
add_compile_options(
    -ffile-prefix-map=${CMAKE_SOURCE_DIR}=/src
    -fmacro-prefix-map=${CMAKE_SOURCE_DIR}=/src)

# The NDK hardens every build with -Werror=format-security, which turns the one
# pre-existing warning in the tree -- fgame/trigger.cpp:2922, retail's own
# non-literal message string, already sanctioned as-is on the desktop build --
# into an error.  Downgrade it back to the warning the desktop build carries;
# the retail text stays untouched.
add_compile_options(-Wno-error=format-security)

# ---------------------------------------------------------------------------
# Every shared module is packaged by the app build, and the Android packager
# only accepts the "lib<name>.so" spelling (it is also what the linker's own
# search path finds, and what the engine's loader asks for on this platform --
# see Sys_LoadDll's __ANDROID__ branch).  The desktop build keeps the empty
# prefix and its retail file names (game.so, cgame.so, renderer_gles3.so).
#
# The desktop DEBUG_POSTFIX ("-dbg", compilers/all.cmake) would break that: the
# packaged file would be libgame-dbg.so while the engine asks for libgame.so.
# A debug-configuration device build keeps the same file names and differs only
# in optimisation/defines, exactly as on the desktop.
# ---------------------------------------------------------------------------
set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
set(CMAKE_DEBUG_POSTFIX "")
# SDL2's own equivalent of the same switch.
set(SDL_CMAKE_DEBUG_POSTFIX "")

# ---------------------------------------------------------------------------
# The one data root.  fs_basepath == fs_homepath == the app's own OBB
# directory, which needs no storage permission at any targetSdk and is the
# directory the launcher activity both extracts the bundled files into and
# imports the retail paks into.  Baked at build time -- the same single
# compile-time root EF1 bakes (ANDROID_DATA_ROOT) -- so the engine side and the
# Java side cannot disagree about where the game lives.
#
# MUST match applicationId in the gradle project (app/build.gradle).
# Retail's game directory ("base", q_shared.h BASEGAME) lives directly beneath
# it:  <root>/base/*.pk3  -- the same layout retail ships on the desktop.
# ---------------------------------------------------------------------------
add_compile_definitions(ANDROID_DATA_ROOT="/storage/emulated/0/Android/obb/com.provenanceef2")

# ---------------------------------------------------------------------------
# The platform sources.  Android is Linux underneath, so platforms/unix.cmake
# has already added the unix sys backend (sys_unix.c) and the tty console; the
# tty console is meaningless without a terminal, so it is swapped for the
# passive one (the same console EF1's Android build uses).  The Android-specific
# half of these files is the __ANDROID__ branch each carries (home path, base
# path, module-name resolution).
# ---------------------------------------------------------------------------
list(REMOVE_ITEM SYSTEM_PLATFORM_SOURCES ${SOURCE_DIR}/sys/con_tty.c)
list(APPEND SYSTEM_PLATFORM_SOURCES
    ${SOURCE_DIR}/sys/con_passive.c)

# Android also needs liblog for the engine's own __android_log_print output.
find_library(EF2_ANDROID_LOG_LIB log)
if(EF2_ANDROID_LOG_LIB)
    list(APPEND COMMON_LIBRARIES ${EF2_ANDROID_LOG_LIB})
endif()

# The engine target is a shared library, not an executable; the gradle/AGP
# build packages every SHARED target in this project into the APK
# automatically (libmain.so, libSDL2.so, libgame.so, libcgame.so,
# librenderer_gles3.so, libopenal.so, libc++_shared.so).
set(EF2_ANDROID_CLIENT_LIBRARY ON)
