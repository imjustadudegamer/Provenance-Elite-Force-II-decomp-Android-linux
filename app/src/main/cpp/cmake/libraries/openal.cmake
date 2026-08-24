if(NOT USE_OPENAL)
    return()
endif()

if(NOT BUILD_CLIENT)
    return()
endif()

set(INTERNAL_OPENAL_DIR ${SOURCE_DIR}/thirdparty/openal-soft-1.24.3)

# ---------------------------------------------------------------------------
# ANDROID (r133-android): the audio backend is a platform layer, and on the
# device that layer is OpenAL Soft (it speaks AAudio/OpenSL ES on Android, so
# the engine's OpenAL path is untouched above it).  There is no system OpenAL
# to find_package() under the NDK, so the 1.24.3 source is carried in-tree --
# the exact version of the headers the engine already vendors
# (thirdparty/openal-soft-1.24.3/include) -- and built as libopenal.so next to
# the engine, which is where the engine's own loader looks for it
# (snd_openal_new.c ALDRIVER_DEFAULT, see the __ANDROID__ branch).
#
# Only the library: no utility/example programs (they need a host GL/FFmpeg),
# no install rules, no git-version probe.
# ---------------------------------------------------------------------------
if(ANDROID)
    set(LIBTYPE SHARED CACHE STRING "" FORCE)
    set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
    set(ALSOFT_NO_CONFIG_UTIL ON CACHE BOOL "" FORCE)
    set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ALSOFT_TESTS OFF CACHE BOOL "" FORCE)
    set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
    set(ALSOFT_INSTALL_CONFIG OFF CACHE BOOL "" FORCE)
    set(ALSOFT_INSTALL_HRTF_DATA OFF CACHE BOOL "" FORCE)
    set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE BOOL "" FORCE)
    set(ALSOFT_INSTALL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ALSOFT_INSTALL_UTILS OFF CACHE BOOL "" FORCE)
    set(ALSOFT_UPDATE_BUILD_VERSION OFF CACHE BOOL "" FORCE)
    set(ALSOFT_DLOPEN OFF CACHE BOOL "" FORCE)
    # RTKit is the desktop D-Bus real-time-priority path; its pkg-config probe
    # finds the HOST's dbus and drags -ldbus-1 into the device .so.
    set(ALSOFT_RTKIT OFF CACHE BOOL "" FORCE)
    # Every desktop backend is pinned OFF: their probes run pkg-config against
    # the HOST (/usr/lib/x86_64-linux-gnu), so an unguarded cross build silently
    # links libdbus-1.so and friends into a device .so that can never load them.
    # The device build keeps OpenSL (and the null backend as the fallback); the
    # AAudio/Oboe backend needs the oboe source tree, which is not vendored.
    set(ALSOFT_BACKEND_PIPEWIRE OFF CACHE BOOL "" FORCE)
    set(ALSOFT_BACKEND_PULSEAUDIO OFF CACHE BOOL "" FORCE)
    set(ALSOFT_BACKEND_ALSA OFF CACHE BOOL "" FORCE)
    set(ALSOFT_BACKEND_OSS OFF CACHE BOOL "" FORCE)
    set(ALSOFT_BACKEND_JACK OFF CACHE BOOL "" FORCE)
    set(ALSOFT_BACKEND_PORTAUDIO OFF CACHE BOOL "" FORCE)
    set(ALSOFT_BACKEND_SNDIO OFF CACHE BOOL "" FORCE)
    set(ALSOFT_BACKEND_SOLARIS OFF CACHE BOOL "" FORCE)
    set(ALSOFT_BACKEND_OBOE OFF CACHE BOOL "" FORCE)
    add_subdirectory(${INTERNAL_OPENAL_DIR} ${CMAKE_BINARY_DIR}/openal EXCLUDE_FROM_ALL)

    list(APPEND CLIENT_DEFINITIONS USE_OPENAL)
    list(APPEND CLIENT_INCLUDE_DIRS ${INTERNAL_OPENAL_DIR}/include)
    # USE_OPENAL_DLOPEN stays ON: the engine resolves libopenal.so at run time
    # exactly as it does on the desktop, so the module boundary (and the
    # s_openaldriver cvar) is the same on both sides of the platform switch.
    list(APPEND CLIENT_DEFINITIONS USE_OPENAL_DLOPEN)
    return()
endif()

find_package(OpenAL QUIET)

if(NOT OpenAL_FOUND)
    set(OPENAL_DEFINITIONS USE_INTERNAL_OPENAL_HEADERS)
    set(OPENAL_INCLUDE_DIR ${INTERNAL_OPENAL_DIR}/include)
    set(OPENAL_LIBRARY openal)
endif()

list(APPEND CLIENT_DEFINITIONS ${OPENAL_DEFINITIONS} USE_OPENAL)
list(APPEND CLIENT_INCLUDE_DIRS ${OPENAL_INCLUDE_DIR})

if(USE_OPENAL_DLOPEN)
    list(APPEND CLIENT_DEFINITIONS USE_OPENAL_DLOPEN)
else()
    find_package(Threads REQUIRED)
    list(APPEND CLIENT_LIBRARIES Threads::Threads ${OPENAL_LIBRARY})
endif()
