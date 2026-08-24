if(NOT BUILD_CLIENT)
    return()
endif()

include(utils/add_git_dependency)
include(utils/set_output_dirs)
include(shared_sources)
include(shared_script)

include(renderer_common)

set(CLIENT_SOURCES
    ${SOURCE_DIR}/client/cl_avi.cpp
    ${SOURCE_DIR}/client/cl_cgame.cpp
    ${SOURCE_DIR}/client/cl_cin.cpp
    ${SOURCE_DIR}/client/cl_consolecmds.cpp
    ${SOURCE_DIR}/client/cl_input.cpp
    ${SOURCE_DIR}/client/cl_inv.cpp
    ${SOURCE_DIR}/client/cl_invrender.cpp
    ${SOURCE_DIR}/client/cl_keys.cpp
    ${SOURCE_DIR}/client/cl_main.cpp
    ${SOURCE_DIR}/client/cl_net_chan.cpp
    ${SOURCE_DIR}/client/cl_parse.cpp
    ${SOURCE_DIR}/client/cl_scrn.cpp
    ${SOURCE_DIR}/client/cl_scoreboard.cpp
    ${SOURCE_DIR}/client/cl_systeminfo.cpp
    ${SOURCE_DIR}/client/cl_ui.cpp
    ${SOURCE_DIR}/client/cl_uibranchdialog.cpp
    ${SOURCE_DIR}/client/cl_uidialog.cpp
    ${SOURCE_DIR}/client/cl_uifilepicker.cpp
    ${SOURCE_DIR}/client/cl_savemanager.cpp
    ${SOURCE_DIR}/client/cl_uiloadsave.cpp
    ${SOURCE_DIR}/client/cl_uimaprunner.cpp
    ${SOURCE_DIR}/client/cl_uiserverlist.cpp
    ${SOURCE_DIR}/client/cl_uisoundpicker.cpp
    ${SOURCE_DIR}/client/cl_uistd.cpp
    ${SOURCE_DIR}/client/cl_uiview3d.cpp
    ${SOURCE_DIR}/client/qal.c
    ${SOURCE_DIR}/client/snd_codec_mp3.c
    ${SOURCE_DIR}/client/snd_codec_wav.c
    ${SOURCE_DIR}/client/snd_codec.c
    ${SOURCE_DIR}/client/snd_dma_new.cpp
    ${SOURCE_DIR}/client/snd_info.cpp
    ${SOURCE_DIR}/client/snd_mem_new.cpp
    ${SOURCE_DIR}/client/snd_miles_new.cpp
    ${SOURCE_DIR}/client/snd_openal_new.cpp
    ${SOURCE_DIR}/client/usignal.cpp
    ${SOURCE_DIR}/sdl/sdl_input.c
    ${SOURCE_DIR}/sdl/sdl_mouse.c
    ${CLIENT_PLATFORM_SOURCES}
)

# Gamespy
list(APPEND CLIENT_SOURCES
	${SOURCE_DIR}/gamespy/cl_gamespy.c
)

# r133-mt (docs/r133-mt_design.md T2/T3): the client module owns its own
# private job-pool instance (the sound feed and the IO workstream are its consumers).
# It is deliberately NOT in COMMON_SOURCES: the dedicated server does not link
# SDL2 and must not grow a threading dependency it never asked for.
list(APPEND CLIENT_SOURCES ${SOURCE_DIR}/qcommon/q_jobs.c)

file(GLOB_RECURSE UI_SOURCES "${SOURCE_DIR}/uilib/*.c" "${SOURCE_DIR}/uilib/*.cpp")

add_git_dependency(${SOURCE_DIR}/client/cl_console.c)

set(CLIENT_BINARY ${CLIENT_NAME})

list(APPEND CLIENT_DEFINITIONS BOTLIB)
list(APPEND CLIENT_DEFINITIONS APP_MODULE)

if(BUILD_STANDALONE)
    list(APPEND CLIENT_DEFINITIONS STANDALONE)
endif()

if(USE_RENDERER_DLOPEN)
    list(APPEND CLIENT_DEFINITIONS USE_RENDERER_DLOPEN)
endif()

# The renderer/platform switch (top-level CMakeLists): pass the selected
# cl_renderer default through as a quoted literal.  It is "gles3", the literal
# the one remaining renderer module (renderer_gles3.so) is selected by.
list(APPEND CLIENT_DEFINITIONS EF2_DEFAULT_RENDERER="${EF2_DEFAULT_RENDERER}")

list(APPEND CLIENT_BINARY_SOURCES
    ${SERVER_SOURCES}
    ${CLIENT_SOURCES}
    ${UI_SOURCES}
    ${COMMON_SOURCES}
    ${SCRIPT_SYSTEM_SOURCES}
    ${BOTLIB_SOURCES}
    ${SYSTEM_SOURCES}
    ${ASM_SOURCES}
    ${CLIENT_LIBRARY_SOURCES})

if(EF2_ANDROID_CLIENT_LIBRARY)
    # THE DEVICE TARGET (r133-android, cmake/platforms/android.cmake): the engine
    # ships as libmain.so, which SDLActivity loads and calls SDL_main() on -- the
    # EF1 shipping pattern ("Rename sys_main.c's main() to SDL_main so
    # SDLActivity's bootstrap calls it", EF1 app/jni/src/CMakeLists.txt).  The
    # sys_main.c rename is done at the source-file level so the desktop build's
    # compile of that one TU is untouched; SDL.h's own SDL_main.h would do the
    # same thing for __ANDROID__, this makes it explicit and independent of
    # whether the TU ends up including SDL.h.
    set_source_files_properties(${SOURCE_DIR}/sys/sys_main.c PROPERTIES
        COMPILE_DEFINITIONS "main=SDL_main")

    add_library(${CLIENT_BINARY} SHARED ${CLIENT_BINARY_SOURCES})
    set_target_properties(${CLIENT_BINARY} PROPERTIES OUTPUT_NAME main)
else()
    add_executable(${CLIENT_BINARY} ${CLIENT_EXECUTABLE_OPTIONS} ${CLIENT_BINARY_SOURCES})
endif()

target_include_directories(     ${CLIENT_BINARY} PRIVATE ${CLIENT_INCLUDE_DIRS})
target_include_directories(     ${CLIENT_BINARY} PRIVATE ${SOURCE_DIR}/client)
target_compile_definitions(     ${CLIENT_BINARY} PRIVATE ${CLIENT_DEFINITIONS})
target_compile_options(         ${CLIENT_BINARY} PRIVATE ${CLIENT_COMPILE_OPTIONS})
target_link_libraries(          ${CLIENT_BINARY} PRIVATE ${COMMON_LIBRARIES} ${CLIENT_LIBRARIES})
target_link_options(            ${CLIENT_BINARY} PRIVATE ${CLIENT_LINK_OPTIONS})

# (The game module does NOT import Gameplay* symbols from the host: retail
# compiles the Shared/qcommon gameplay sources into each binary -- see
# basegame.cmake GAME_MODULE_SHARED_SOURCES.)

set_output_dirs(${CLIENT_BINARY})

if(NOT USE_RENDERER_DLOPEN)
    target_sources(${CLIENT_BINARY} PRIVATE
        ${RENDERER_GLES3_BINARY_SOURCES})

    target_include_directories( ${CLIENT_BINARY} PRIVATE ${RENDERER_INCLUDE_DIRS})
    target_compile_definitions( ${CLIENT_BINARY} PRIVATE ${RENDERER_DEFINITIONS})
    target_compile_options(     ${CLIENT_BINARY} PRIVATE ${RENDERER_COMPILE_OPTIONS})
    target_link_libraries(      ${CLIENT_BINARY} PRIVATE ${RENDERER_LIBRARIES})
endif()

foreach(LIBRARY IN LISTS CLIENT_DEPLOY_LIBRARIES)
    add_custom_command(TARGET ${CLIENT_BINARY} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${LIBRARY}
            $<TARGET_FILE_DIR:${CLIENT_BINARY}>)

    install(FILES ${LIBRARY} DESTINATION
        # install() requires a relative path hence:
        $<PATH:RELATIVE_PATH,$<TARGET_FILE_DIR:${CLIENT_BINARY}>,${CMAKE_BINARY_DIR}/$<CONFIG>>)
endforeach()

# The Android build clears CMAKE_DEBUG_POSTFIX (cmake/platforms/android.cmake):
# the app packager needs the module's file name to stay lib<name>.so in every
# configuration, so the empty-postfix case skips the property entirely.
if(CMAKE_DEBUG_POSTFIX)
    set_target_properties(${CLIENT_BINARY} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
endif()

INSTALL(TARGETS ${CLIENT_BINARY} DESTINATION ${INSTALL_BINDIR_FULL})

if (MSVC)
	target_link_options(${CLIENT_BINARY} PRIVATE "/MANIFEST:NO")
	INSTALL(FILES $<TARGET_PDB_FILE:${CLIENT_BINARY}> DESTINATION ${INSTALL_BINDIR_FULL} OPTIONAL)
endif()
