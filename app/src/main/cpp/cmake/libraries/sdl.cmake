if(NOT BUILD_CLIENT)
    return()
endif()

if(EMSCRIPTEN)
    # Emscripten provides its own self contained SDL setup
    list(APPEND CLIENT_COMPILE_OPTIONS -sUSE_SDL=2)
    list(APPEND CLIENT_LINK_OPTIONS -sUSE_SDL=2)
    return()
endif()

set(INTERNAL_SDL_DIR ${SOURCE_DIR}/thirdparty/SDL2-2.32.8)

# ---------------------------------------------------------------------------
# ANDROID (uivk-android): build the vendored SDL2 in-tree as libSDL2.so.
#
# SDL2 is the whole Android platform surface for this port -- the activity
# (org.libsdl.app.SDLActivity, driven by the app's own subclass), the window,
# the event pump, the audio device and the GLES 3.0 context
# (SDL_CreateWindow(SDL_WINDOW_OPENGL) + SDL_GL_CreateContext).  There is no
# system SDL2 to find_package() under the NDK, so the
# tree carries the same upstream SDL2 the EF1 Android port ships
# (2.30.9, device-proven there; unmodified, license in the directory).
#
# Trimmed to what the engine can reach: no SDL2main (SDLActivity is the main),
# no static lib, no test lib, no SDL_Renderer (the engine renders through its own
# backend, not SDL's 2D renderer).
#
# gles3-gles3 ROOT FIX -- SDL_OPENGLES must be ON.  The GLES 3.0 renderer creates
# its context through SDL (gl_gles3_platform.c: SDL_CreateWindow(SDL_WINDOW_OPENGL)
# + SDL_GL_CreateContext), and SDL's Android video driver installs its GL entry
# points -- GL_LoadLibrary/GL_GetProcAddress/GL_CreateContext/GL_MakeCurrent/
# GL_SwapWindow -- ONLY under `#ifdef SDL_VIDEO_OPENGL_EGL`
# (src/video/android/SDL_androidvideo.c:131-142).  With SDL_OPENGLES OFF that
# define is unset, `_this->GL_CreateContext` stays NULL, and every
# SDL_CreateWindow(SDL_WINDOW_OPENGL) fails with
#     "OpenGL support is either not configured in SDL or not available in
#      current SDL video driver (Android) or platform"
# which R_Init turns into "GLimp_Init() - could not create a GLES 3.0 subsystem".
# SDL_OPENGL (DESKTOP GL) stays OFF -- Android is ES/EGL only.  The option
# mapping is SDL's own: SDL_OPENGLES -> SDL_VIDEO_OPENGL_ES2 +
# SDL_VIDEO_OPENGL_EGL (thirdparty/SDL2-2.30.9/CMakeLists.txt:2000-2002).
# SDL_VULKAN is ON: renderer_vulkan (vulkan-m0) opens its window with
# SDL_WINDOW_VULKAN and its surface with SDL_Vulkan_CreateSurface on Android
# too, which needs SDL's android/SDL_androidvulkan.c compiled in.
# ---------------------------------------------------------------------------
if(ANDROID)
    set(SDL2_ANDROID_DIR ${SOURCE_DIR}/thirdparty/SDL2-2.30.9)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
    set(SDL_RENDER OFF CACHE BOOL "" FORCE)
    set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
    set(SDL_OPENGLES ON CACHE BOOL "" FORCE)
    set(SDL_VULKAN ON CACHE BOOL "" FORCE)
    # bionic has clock_gettime in libc and no librt to link; SDL's Linux probe
    # (check_library_exists(rt ...)) still "finds" one and would add -lrt to
    # every target that links SDL2.  Android's own timer path never consults
    # this switch.
    set(SDL_CLOCK_GETTIME OFF CACHE BOOL "" FORCE)
    add_subdirectory(${SDL2_ANDROID_DIR} ${CMAKE_BINARY_DIR}/sdl2 EXCLUDE_FROM_ALL)
    # uivk: keep the upstream tree pristine -- two clang-only unused-variable warnings
    # in SDL's own android video/core backends are silenced at the target level.
    # uiparity: +deprecated-declarations -- SDL 2.30.9's android sensor backend calls
    # ASensorManager_getInstance, deprecated in NDK r25 for API >= 26 while upstream
    # still supports older levels.  Vendored, so silenced at the target, not patched.
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(SDL2 PRIVATE -Wno-unused-variable -Wno-deprecated-declarations)
    endif()

    set(SDL2_LIBRARIES SDL2::SDL2)
    set(SDL2_INCLUDE_DIRS ${SDL2_ANDROID_DIR}/include)

    list(APPEND CLIENT_LIBRARIES ${SDL2_LIBRARIES})
    list(APPEND CLIENT_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS})
    list(APPEND RENDERER_LIBRARIES ${SDL2_LIBRARIES})
    list(APPEND RENDERER_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS})
    return()
endif()

include(utils/arch)

if(WIN32)
    # On Windows we have internal SDL binaries we can use
    set(HAVE_INTERNAL_SDL true)
endif()

if(USE_INTERNAL_SDL AND HAVE_INTERNAL_SDL)
    set(SDL2_INCLUDE_DIRS ${INTERNAL_SDL_DIR}/include)
    list(APPEND CLIENT_DEFINITIONS USE_INTERNAL_SDL_HEADERS)
    list(APPEND RENDERER_DEFINITIONS USE_INTERNAL_SDL_HEADERS)

    if(WIN32)
        if(ARCH STREQUAL "x86_64")
            set(LIB_DIR ${SOURCE_DIR}/thirdparty/libs/win64)
        elseif(ARCH STREQUAL "x86")
            set(LIB_DIR ${SOURCE_DIR}/thirdparty/libs/win32)
        else()
            message(FATAL_ERROR "Unknown ARCH")
        endif()

        if(MINGW)
            set(SDL2_LIBRARIES
                ${LIB_DIR}/libSDL2main.a
                ${LIB_DIR}/libSDL2.dll.a)
        elseif(MSVC)
            set(SDL2_LIBRARIES
                ${LIB_DIR}/SDL2main.lib
                ${LIB_DIR}/SDL2.lib)
        endif()

        list(APPEND CLIENT_DEPLOY_LIBRARIES ${LIB_DIR}/SDL2.dll)
    else()
        message(FATAL_ERROR "HAVE_INTERNAL_SDL set incorrectly; file a bug")
    endif()
else()
    find_package(SDL2 REQUIRED)
endif()

list(APPEND CLIENT_LIBRARIES ${SDL2_LIBRARIES})
list(APPEND CLIENT_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS})
list(APPEND CLIENT_COMPILE_OPTIONS ${SDL2_CFLAGS_OTHER})
list(APPEND RENDERER_LIBRARIES ${SDL2_LIBRARIES})
list(APPEND RENDERER_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS})
list(APPEND RENDERER_COMPILE_OPTIONS ${SDL2_CFLAGS_OTHER})
