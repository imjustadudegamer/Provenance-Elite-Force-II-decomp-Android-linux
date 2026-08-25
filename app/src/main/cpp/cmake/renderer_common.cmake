include_guard(GLOBAL)

set(RENDERER_COMMON_SOURCES
    ${SOURCE_DIR}/renderercommon/tr_font.c
    ${SOURCE_DIR}/renderercommon/tr_image_bmp.c
    ${SOURCE_DIR}/renderercommon/tr_image_jpg.c
    ${SOURCE_DIR}/renderercommon/tr_image_pcx.c
    ${SOURCE_DIR}/renderercommon/tr_image_png.c
    ${SOURCE_DIR}/renderercommon/tr_image_pvr.c
    ${SOURCE_DIR}/renderercommon/tr_image_tga.c
    ${SOURCE_DIR}/renderercommon/tr_noise.c
    ${SOURCE_DIR}/renderercommon/puff.c
	${SOURCE_DIR}/tiki/tiki_mesh.cpp
)

set(SDL_RENDERER_SOURCES
    ${SOURCE_DIR}/sdl/sdl_gamma.c
    ${SOURCE_DIR}/sdl/sdl_glimp.c
)

set(DYNAMIC_RENDERER_SOURCES
    ${SOURCE_DIR}/renderercommon/tr_subs.c
    ${SOURCE_DIR}/qcommon/q_shared.c
    ${SOURCE_DIR}/qcommon/q_math.c
    ${SOURCE_DIR}/qcommon/q_jobs.c
    ${SOURCE_DIR}/corepp/str.cpp
)

# uivk-mt (docs/uivk-mt_design.md T2): the job pool is compiled INTO each
# renderer module, exactly like q_shared.c/q_math.c above -- one private pool
# instance per module, no refimport_t slot, no cross-.so symbol.  The workers
# are spawned by the renderer's own R_Init and joined by its own RE_Shutdown.
list(APPEND RENDERER_COMMON_SOURCES ${SOURCE_DIR}/qcommon/q_jobs.c)

if(USE_FREETYPE)
    list(APPEND RENDERER_DEFINITIONS BUILD_FREETYPE)
endif()

if(USE_RENDERER_DLOPEN)
    list(APPEND RENDERER_DEFINITIONS USE_RENDERER_DLOPEN)
    list(APPEND RENDERER_DEFINITIONS REF_DLL=1)
elseif(NOT BUILD_RENDERER_GLES3)
    # There is only ONE renderer: retail EF2 shipped a single renderer (the Mac
    # build fixed-function, the PC build D3D), and this port's GLES 3.0 backend
    # is the sanctioned platform layer for it.  The fixed-function and Vulkan
    # backends, and the ioq3 "GL2" renderer (52 files retail never had), are all
    # deleted; a static (non-dlopen) build must select the one that remains.
    message(FATAL_ERROR "No static renderer enabled; BUILD_RENDERER_GLES3 is required")
endif()

# Keep absolute build paths out of the shipped renderer module, because the
# rule is about what lands in the .so, not about the host.
# ANDROID_VULKAN_RENDERER_SDK.md §15: /home/<user>/... leaks into stripped
# libraries and into every appErrorf/assert message unless __FILE__ and the
# macro/debug paths are remapped project-relative.  Verified there as
# "0 such strings after"; here it is a no-op for behaviour, so the renderer
# keeps every diagnostic it prints today -- only the path prefix inside the
# string changes.
list(APPEND RENDERER_COMPILE_OPTIONS
    "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."
    "-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.")

list(APPEND RENDERER_LIBRARIES ${COMMON_LIBRARIES})

