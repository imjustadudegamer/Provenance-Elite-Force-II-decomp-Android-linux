# renderer_gles3.cmake -- the GLES 3.0 renderer (gles3-gles3, G1).
#
# This target is the programme's shipping renderer and the ONLY one: it is a
# FORK of the fixed-function renderer's sources with its substrate swapped:
#
#     fixed-function desktop GL  ->  GLES 3.0 + GLSL ES 3.0
#     GLX via SDL_opengl.h       ->  EGL/SDL GLES3 (Android + Linux desktop)
#     sdl/sdl_glimp.c bindings   ->  renderergles3/qgl_gles3.c (named-missing
#                                    loader) + renderergles3/gl_gles3_platform.c
#
# Source provenance: every forked file listed below descends from the
# fixed-function renderer's same-named file at the programme switch commit.
# That renderer (the former behaviour oracle) is deleted by decision; its
# sources remain reconstructible from git history at the commit that precedes
# the deletion.  This target's own platform layer is:
#   qgl.h / qgl_gles3.[ch] / qgl_gles3_procs.h / qgl_gles3_tokens.h
#   gl_gles3_platform.c   -- this target's platform layer
#   gles3_firstlight.c    -- the provable first-light bootstrap
#   tools/gen_qgl_gles3.py -- the loader's generated proc list
# The coverage matrix in docs/notes/GLES3_CORE_GLES3.md records, per translation
# unit, whether its arms were already ES3-clean or were replaced by the G2
# (shade/shader) and G3 (geometry) changes.
#
# Configuration (app/src/main/cpp/CMakeLists.txt EF2_RENDERER):
#   gles3       -> this target; cl_renderer defaults to "gles3"

if(NOT BUILD_CLIENT OR NOT BUILD_RENDERER_GLES3)
    return()
endif()

include(utils/set_output_dirs)
include(renderer_common)

set(RENDERER_GLES3_SOURCES
    # -- G1-owned translation units -----------------------------------------
    ${SOURCE_DIR}/renderergles3/tr_backend.c
    ${SOURCE_DIR}/renderergles3/tr_cmds.c
    ${SOURCE_DIR}/renderergles3/tr_image.c
    ${SOURCE_DIR}/renderergles3/tr_hwtex.c
    ${SOURCE_DIR}/renderergles3/tr_texcache.c
    ${SOURCE_DIR}/renderergles3/tr_init.c
    ${SOURCE_DIR}/renderergles3/qgl_gles3.c
    ${SOURCE_DIR}/renderergles3/gl_gles3_platform.c
    # smp-mcore: the render-thread layer + the EGL/GL context handshake.
    ${SOURCE_DIR}/renderergles3/gles3_renderthread.c
    ${SOURCE_DIR}/renderergles3/gles3_firstlight.c
    # -- forked oracle translation units (G2/G3 own their conversion) --------
    ${SOURCE_DIR}/renderergles3/tr_bsp.c
    ${SOURCE_DIR}/renderergles3/tr_curve.c
    ${SOURCE_DIR}/renderergles3/tr_draw.c
    ${SOURCE_DIR}/renderergles3/tr_font.cpp
    ${SOURCE_DIR}/renderergles3/tr_ghost.cpp
    ${SOURCE_DIR}/renderergles3/tr_light.c
    ${SOURCE_DIR}/renderergles3/tr_main.c
    ${SOURCE_DIR}/renderergles3/tr_marks.c
    ${SOURCE_DIR}/renderergles3/tr_model.cpp
    ${SOURCE_DIR}/renderergles3/tr_scene.c
    ${SOURCE_DIR}/renderergles3/tr_shade_calc.c
    ${SOURCE_DIR}/renderergles3/tr_shade.c
    ${SOURCE_DIR}/renderergles3/tr_shader.c
    # -- G2's program/state layer -------------------------------------------
    # tr_program.c is the GLSL ES 3.0 program/state layer tr_shade.c drives;
    # without it the merged module links with every GLES3_* symbol undefined.
    ${SOURCE_DIR}/renderergles3/tr_program.c
    # progcache-progcache: the disk-persisted program cache.  tr_program.c drives
    # it; the file itself is GL- and engine-free above its adapter, so the
    # progcache_progcache_probe below links THIS source with a fake driver and a
    # host filesystem and certifies the code that ships.
    ${SOURCE_DIR}/renderergles3/tr_program_cache.c
    ${SOURCE_DIR}/renderergles3/tr_shadows.c
    ${SOURCE_DIR}/renderergles3/tr_sky_portal.cpp
    ${SOURCE_DIR}/renderergles3/tr_sky.c
    ${SOURCE_DIR}/renderergles3/tr_sphere_shade.cpp
    ${SOURCE_DIR}/renderergles3/tr_sprite.c
    ${SOURCE_DIR}/renderergles3/tr_staticmodels.cpp
    ${SOURCE_DIR}/renderergles3/tr_sun_flare.cpp
    ${SOURCE_DIR}/renderergles3/tr_surface.c
    ${SOURCE_DIR}/renderergles3/tr_swipe.cpp
    ${SOURCE_DIR}/renderergles3/tr_util.cpp
    # -- G3's VBO/stream substrate ------------------------------------------
    # tr_vbo.c is the glBegin/glEnd replacement (tr_vbo.h is G3's interface)
    # and tr_main.c/tr_surface.c call into it; tr_world.c is the forked
    # oracle world path (R_AddWorldSurfaces and friends) without which the
    # backend has no world to draw.  Both are needed for the module to link.
    ${SOURCE_DIR}/renderergles3/tr_vbo.c
    ${SOURCE_DIR}/renderergles3/tr_world.c
)

set(RENDERER_GLES3_BASENAME renderer_gles3)
set(RENDERER_GLES3_BINARY ${RENDERER_GLES3_BASENAME})

# The platform layer.  The deleted fixed-function renderer appended
# ${SDL_RENDERER_SOURCES} (sdl/sdl_glimp.c + sdl/sdl_gamma.c); sdl_glimp.c must
# NOT be used here -- it defines every qgl* pointer with desktop bindings and
# has no ES 3.0 context entry (see gl_gles3_platform.c's header).  sdl_gamma.c
# is renderer-agnostic (SDL window + glConfig + r_gamma) and is reused verbatim.
list(APPEND RENDERER_GLES3_BINARY_SOURCES
    ${RENDERER_COMMON_SOURCES}
    ${RENDERER_GLES3_SOURCES}
    ${SOURCE_DIR}/sdl/sdl_gamma.c
    ${RENDERER_LIBRARY_SOURCES})

# ---------------------------------------------------------------------------
# GLES/EGL link.  On Android both come from the NDK sysroot; on Linux from
# mesa's libGLESv2 + libEGL.  Neither is optional for this target: without
# them the loader's resolver has nothing to resolve against.
# ---------------------------------------------------------------------------
if(ANDROID)
    list(APPEND RENDERER_GLES3_LIBRARIES GLESv2 EGL)
else()
    find_library(EF2_GLES3_LIBRARY NAMES GLESv2)
    find_library(EF2_EGL_LIBRARY NAMES EGL)
    if(NOT EF2_GLES3_LIBRARY OR NOT EF2_EGL_LIBRARY)
        message(FATAL_ERROR
            "EF2_RENDERER=gles3 needs libGLESv2 and libEGL (mesa's "
            "libgles2-mesa-dev / libegl1-mesa-dev).  Found GLESv2='${EF2_GLES3_LIBRARY}' "
            "EGL='${EF2_EGL_LIBRARY}'.")
    endif()
    list(APPEND RENDERER_GLES3_LIBRARIES ${EF2_GLES3_LIBRARY} ${EF2_EGL_LIBRARY})
endif()

if(USE_RENDERER_DLOPEN)
    list(APPEND RENDERER_GLES3_BINARY_SOURCES ${DYNAMIC_RENDERER_SOURCES})

    add_library(${RENDERER_GLES3_BINARY} SHARED ${RENDERER_GLES3_BINARY_SOURCES})

    target_link_libraries(      ${RENDERER_GLES3_BINARY} PRIVATE
                                ${RENDERER_LIBRARIES} ${RENDERER_GLES3_LIBRARIES})
    target_include_directories( ${RENDERER_GLES3_BINARY} PRIVATE ${RENDERER_INCLUDE_DIRS})
    target_compile_definitions( ${RENDERER_GLES3_BINARY} PRIVATE
                                ${RENDERER_DEFINITIONS}
                                # The substrate switch: renderercommon/qgl.h
                                # pulls the GLES3 headers and the loader's
                                # generated proc list is compiled in.
                                EF2_GLES3=1
                                # The deleted fixed-function renderer's platform
                                # layer is not compiled here; this keeps any
                                # shared code that tests it honest.
                                EF2_RENDERER_GLES3=1
                                # smp-smp2 (H3): the bone arena is one slot per frame in
                                # flight.  The GLES3 frame pair publishes one record per
                                # slot (tr_cmds.c rb_frames[]: `frame->boneArena = tr.smpFrame`,
                                # `frame->smpFrame = tr.smpFrame`) and the back end skins
                                # from the slot the executing record latched
                                # (RB_FrameBoneArena() -> rb_activeFrame->boneArena, read by
                                # tr_model.cpp RB_SkelMesh at TIKI_Skel_Bones_Arena[arena]).
                                # The front end keeps solving into the arena of the frame it
                                # is building (tr.smpFrame).  With ONE arena those two indices
                                # name the same object, and a second in-flight frame would
                                # overwrite the poses the executing frame is still reading.
                                TIKI_BONE_ARENAS=2)
    target_compile_options(     ${RENDERER_GLES3_BINARY} PRIVATE
                                ${RENDERER_COMPILE_OPTIONS}
                                -fno-strict-aliasing)
    target_link_options(        ${RENDERER_GLES3_BINARY} PRIVATE ${RENDERER_LINK_OPTIONS})

    set_output_dirs(${RENDERER_GLES3_BINARY})

	INSTALL(TARGETS ${RENDERER_GLES3_BINARY} DESTINATION ${INSTALL_LIBDIR_FULL})

	if(MSVC)
		INSTALL(FILES $<TARGET_PDB_FILE:${RENDERER_GLES3_BINARY}> DESTINATION ${INSTALL_LIBDIR_FULL} OPTIONAL)
	endif()
endif()

# ---------------------------------------------------------------------------
# THE FIRST-LIGHT PROBE (Linux desktop only).
#
# A real executable that opens a headless EGL GLES 3.0 context, runs the SAME
# loader (qgl_gles3.c) and the SAME first-light core (gles3_firstlight.c) as
# the renderer, and prints the [GLES3:firstlight] rows from actual pixel
# readback.  It exists so the gate proves first light with pixels instead of
# trusting that the marker string 
# ---------------------------------------------------------------------------
if(NOT ANDROID AND NOT EMSCRIPTEN)
    add_executable(gles3_gles3_firstlight
        ${SOURCE_DIR}/renderergles3/gles3_firstlight_probe.c
        ${SOURCE_DIR}/renderergles3/gles3_firstlight.c
        ${SOURCE_DIR}/renderergles3/qgl_gles3.c)
    target_include_directories(gles3_gles3_firstlight PRIVATE ${RENDERER_INCLUDE_DIRS})
    target_compile_definitions(gles3_gles3_firstlight PRIVATE
                               EF2_GLES3=1 GLES3_FIRSTLIGHT_STANDALONE=1)
    target_compile_options(gles3_gles3_firstlight PRIVATE ${RENDERER_COMPILE_OPTIONS})
    target_link_libraries(gles3_gles3_firstlight PRIVATE ${RENDERER_GLES3_LIBRARIES} m)
    set_output_dirs(gles3_gles3_firstlight)

    # smp-hwtex: the transcode probe links the SAME tr_hwtex.c the renderer
    # ships (it has no engine or GL entry-point dependencies of its own), so
    # the bytes it certifies are the bytes the device would get.
    add_executable(smp_hwtex_probe
        ${SOURCE_DIR}/renderergles3/smp_hwtex_probe.c
        ${SOURCE_DIR}/renderergles3/tr_hwtex.c)
    target_include_directories(smp_hwtex_probe PRIVATE ${RENDERER_INCLUDE_DIRS})
    target_compile_definitions(smp_hwtex_probe PRIVATE EF2_GLES3=1)
    target_compile_options(smp_hwtex_probe PRIVATE ${RENDERER_COMPILE_OPTIONS})
    target_link_libraries(smp_hwtex_probe PRIVATE ${RENDERER_GLES3_LIBRARIES} m)
    set_output_dirs(smp_hwtex_probe)

    # smp-texcache: the cache probe links the SAME tr_texcache.c and the SAME
    # tr_hwtex.c chain builder the renderer ships -- tr_texcache.c is compiled
    # with SMP_TEXCACHE_STANDALONE so it drops only the ri adapter, and the
    # probe supplies a host-filesystem IO table.  So the cache logic it certifies (key, header, validation, fallback, eviction) is the
    # logic that ships, and the bytes it round-trips are the bytes the renderer
    # uploads.
    add_executable(smp_texcache_probe
        ${SOURCE_DIR}/renderergles3/smp_texcache_probe.c
        ${SOURCE_DIR}/renderergles3/tr_texcache.c
        ${SOURCE_DIR}/renderergles3/tr_hwtex.c)
    target_include_directories(smp_texcache_probe PRIVATE ${RENDERER_INCLUDE_DIRS})
    target_compile_definitions(smp_texcache_probe PRIVATE EF2_GLES3=1 SMP_TEXCACHE_STANDALONE=1)
    target_compile_options(smp_texcache_probe PRIVATE ${RENDERER_COMPILE_OPTIONS})
    target_link_libraries(smp_texcache_probe PRIVATE ${RENDERER_GLES3_LIBRARIES} m)
    set_output_dirs(smp_texcache_probe)

    # progcache-progcache: the probe links the SAME tr_program_cache.c the renderer
    # ships (compiled with PROGCACHE_PROGCACHE_STANDALONE, which drops only the
    # engine adapter) and drives it through the two vtables with a fake driver
    # and a host filesystem.  So the format, the fingerprint, the key
    # derivation, the restore verification and the fallback it certifies are the ones that ship.
    add_executable(progcache_progcache_probe
        ${SOURCE_DIR}/renderergles3/progcache_probe.c
        ${SOURCE_DIR}/renderergles3/tr_program_cache.c)
    target_include_directories(progcache_progcache_probe PRIVATE ${RENDERER_INCLUDE_DIRS})
    target_compile_definitions(progcache_progcache_probe PRIVATE EF2_GLES3=1 PROGCACHE_PROGCACHE_STANDALONE=1)
    target_compile_options(progcache_progcache_probe PRIVATE ${RENDERER_COMPILE_OPTIONS})
    target_link_libraries(progcache_progcache_probe PRIVATE m)
    set_output_dirs(progcache_progcache_probe)

    # progcache-progcache (real driver): the SAME tr_program_cache.c against a real
    # headless EGL GLES 3.0 context, so the one claim only a driver can settle
    # -- retrievable hint -> delete the shaders -> glGetProgramBinary ->
    # glProgramBinary into a fresh program -> an IDENTICAL interface -- is
    # tested here and not only against the fake driver.  It exits 77 when the
    # host has no ES3 context, and callers treat that as SKIP.
    add_executable(progcache_progcache_gl_probe
        ${SOURCE_DIR}/renderergles3/progcache_gl_probe.c
        ${SOURCE_DIR}/renderergles3/tr_program_cache.c)
    target_include_directories(progcache_progcache_gl_probe PRIVATE ${RENDERER_INCLUDE_DIRS})
    target_compile_definitions(progcache_progcache_gl_probe PRIVATE EF2_GLES3=1 PROGCACHE_PROGCACHE_STANDALONE=1)
    target_compile_options(progcache_progcache_gl_probe PRIVATE ${RENDERER_COMPILE_OPTIONS})
    target_link_libraries(progcache_progcache_gl_probe PRIVATE ${RENDERER_GLES3_LIBRARIES} m)
    set_output_dirs(progcache_progcache_gl_probe)
endif()
