cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0177)
    # Policy CMP0177: install() DESTINATION paths are normalized
    # Introduced in CMake 3.31
    cmake_policy(SET CMP0177 NEW)
endif()

if(POLICY CMP0069)
    # Policy CMP0069: INTERPROCEDURAL_OPTIMIZATION is enforced when enabled.  The vendored
    # libmad (code/thirdparty/libmad/CMakeLists.txt:19 and :178) does not set the policy, so
    # every configure printed two CMake dev warnings asking for exactly this line.
    # `cmake_policy(SET)` alone does NOT reach it: the subproject's own
    # cmake_minimum_required() resets the policy scope, which is precisely what
    # CMAKE_POLICY_DEFAULT_CMP0069 is for -- it is inherited through that reset.  Both are
    # set, so this project's own targets and the vendored subprojects agree.
    cmake_policy(SET CMP0069 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(identity)

project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# THE RENDERER / PLATFORM SWITCH
#
# ONE source tree.  Everything above the substrate (engine front-end, game DLL,
# cgame, tiki, skeletor, uilib) is common; the accepted-divergence layers --
# platform glue, sound, and the video backend -- are what the switches select.
#
#   EF2_PLATFORM   linux (default) | android
#       The *target platform*.  Both renderers create their context through
#       SDL: the GLES 3.0 twin through SDL's Android ES/EGL path on Android and
#       SDL + EGL on Mesa (SDL_OPENGLES must stay ON -- see
#       cmake/libraries/sdl.cmake), the Vulkan renderer through
#       SDL_Vulkan_CreateSurface on both.
#
#   EF2_RENDERER   gles3 (the only value)
#       Gates the GLES 3.0 reference twin's sources.  This switch does NOT
#       select what ships: the Vulkan module (cmake/renderer_vulkan.cmake,
#       BUILD_RENDERER_VULKAN) is built unconditionally beside it, and the
#       renderer `cl_renderer` defaults to is the Vulkan one
#       (EF2_DEFAULT_RENDERER, below).  The switch is kept so a stale cache
#       entry fails loudly instead of silently dropping the twin; the
#       OpenGL 1.x fixed-function renderer (retail's Mac substrate) is deleted
#       by decision.
# ---------------------------------------------------------------------------
set(EF2_PLATFORM "linux" CACHE STRING "Target platform: linux | android")
set_property(CACHE EF2_PLATFORM PROPERTY STRINGS linux android)

set(_EF2_RENDERER_DEFAULT "gles3")
set(EF2_RENDERER "${_EF2_RENDERER_DEFAULT}" CACHE STRING
    "Renderer this build ships: gles3")
set_property(CACHE EF2_RENDERER PROPERTY STRINGS gles3)

if(EF2_RENDERER STREQUAL "gles3")
    set(_EF2_BUILD_GLES3 ON)
else()
    message(FATAL_ERROR
        "EF2_RENDERER must be gles3 (the GLES 3.0 reference twin) -- got '${EF2_RENDERER}'")
endif()

# ---------------------------------------------------------------------------
# THE MULTITHREADING SWITCH (uivk-mt, docs/uivk-mt_design.md T4).
#
# EF2_MULTITHREAD (default ON) builds the worker-pool layer (code/qcommon/
# q_jobs.c) into the client and into each renderer module and turns the
# parallel dispatch on.  EF2_MULTITHREAD=0 compiles the layer away entirely:
# q_jobs.c then answers every call with the inline serial loop, no thread is
# created anywhere and every consumer executes the byte-identical sequential
# code it executed before this change.  That zero-config build is the standing
# proof that the threads are OPTIONAL, and the rollback path if a device
# ever misbehaves.
#
# The simulation is never threaded under either value: fgame, cgame, the
# script interpreter and the server frame stay on the main thread
# unconditionally (the parity law, design doc S0).
# ---------------------------------------------------------------------------
option(EF2_MULTITHREAD "Build the worker-pool threading layer (uivk-mt)" ON)

if(EF2_MULTITHREAD)
    add_compile_definitions(EF2_MULTITHREAD=1)
else()
    add_compile_definitions(EF2_MULTITHREAD=0)
endif()

option(BUILD_SERVER "Build dedicated server" ON)
option(BUILD_CLIENT "Build client" ON)
# GLES 3.0 renderer.  This is the fixed-function renderer's fork with the
# substrate swapped to GLES 3.0 + GLSL ES 3.0 (see cmake/renderer_gles3.cmake).
# It ships as the reference twin beside the Vulkan renderer below, which is
# what `cl_renderer` defaults to; EF2_RENDERER is a one-value switch kept so a
# stale cache entry fails loudly.
option(BUILD_RENDERER_GLES3 "Build GLES 3.0 renderer" ${_EF2_BUILD_GLES3})
option(BUILD_RENDERER_VULKAN "Build the Vulkan renderer (vulkan-m0)" ON)
option(BUILD_GAME_LIBRARIES "Build game module libraries" ON)
option(BUILD_GAME_QVMS "Build game module qvms" ON)
option(BUILD_STANDALONE "Build binaries for standalone games" OFF)

# The renderer the client asks for first (cl_renderer's compiled default,
# code/client/cl_main.cpp).  Vulkan is the shipping renderer (operator order
# 2026-10-10); the GLES3 module stays built beside it as the reference twin
# and is still selectable by the literal "gles3" (cl_main.cpp:2882 builds
# "renderer_%s" from cl_renderer).
set(EF2_DEFAULT_RENDERER "vulkan" CACHE STRING
    "cl_renderer's compiled default")

option(USE_RENDERER_DLOPEN "Dynamically load the renderer(s)" ON)
option(USE_OPENAL "OpenAL audio" ON)
option(USE_OPENAL_DLOPEN "Dynamically load OpenAL" ON)
option(USE_CODEC_MAD "MP3 support" ON)
option(USE_FREETYPE "Freetype font rendering" OFF)

option(USE_INTERNAL_LIBS "Use internally packaged libraries" ON)
option(USE_INTERNAL_SDL "Use internal SDL binary (if available)" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_ZLIB "Use internal copy of zlib" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_JPEG "Use internal copy of libjpeg" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_MAD "Use internal copy of libmad" ${USE_INTERNAL_LIBS})

# Release build by default, set externally if you want something else
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Try to enable LTO by default
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)
if(IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG FALSE)
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/code)

# Enable unit testing
include(CTest)

if(DEFINED ENV{SOURCE_DATE_EPOCH})
    string(TIMESTAMP PRODUCT_DATE "%b %d %Y" UTC)
    add_compile_definitions(PRODUCT_DATE="${PRODUCT_DATE}")
endif()

include(compilers/all)
include(platforms/all)
include(libraries/all)
include(deploy/all)

include(version)

include(server)
include(renderer_gles3)
include(renderer_vulkan)
include(client)
include(basegame)
#include(missionpack)
# OpenMoHAA's multi-game launcher (launch_provenanceef2_{base,spearhead,breakthrough})
# is gone: retail EF2 ships a single executable and the three launcher binaries
# differed only in the `+set com_target_game N` argument they injected, which
# selects a targeting layer this tree no longer has.

include(post_configure)
#include(installer)
