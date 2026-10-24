# renderer_vulkan.cmake -- the Vulkan renderer.
#
# The second renderer target.  Its organisation mirrors renderergles3/ file
# for file (tr_init.c / tr_cmds.c / the platform layer / the loader / the
# first-light core); the Vulkan idioms come from the operator's reference
# set (VoyagerNX EF1 renderervk, the KHG SDK doc, the BC ddraw shim research).
#
# It loads through the same seam as every other renderer: cl_renderer builds
# "renderer_%s", so this module is selected with `+set cl_renderer vulkan`
# and it is the compiled default (EF2_DEFAULT_RENDERER).
#
# No link-time Vulkan dependency: the loader is dlopen'd at run time
# (libvulkan.so.1 on desktop, libvulkan.so on Android) and every proc comes
# off vkGetInstanceProcAddr / vkGetDeviceProcAddr, so a machine without a
# Vulkan driver still boots the shipping renderer.
#
# PURE VULKAN: the target compiles no GL header and calls no gl*(); the
# surface comes from SDL_Vulkan_CreateSurface on both platforms.

if(NOT BUILD_CLIENT OR NOT BUILD_RENDERER_VULKAN)
    return()
endif()

include(utils/set_output_dirs)
include(renderer_common)

set(RENDERER_VULKAN_SOURCES
    ${SOURCE_DIR}/renderervulkan/tr_init.c
    ${SOURCE_DIR}/renderervulkan/tr_cmds.c
    # the data-model layer, mirrored from renderergles3 file for file
    ${SOURCE_DIR}/renderervulkan/tr_shader.c
    ${SOURCE_DIR}/renderervulkan/tr_image.c
    ${SOURCE_DIR}/renderervulkan/tr_font.cpp
    ${SOURCE_DIR}/renderervulkan/tr_draw.c
    ${SOURCE_DIR}/renderervulkan/tr_backend.c
    ${SOURCE_DIR}/renderervulkan/tr_hwtex.c
    ${SOURCE_DIR}/renderervulkan/tr_texcache.c
    # the 3D front end, mirrored from renderergles3 file for file
    ${SOURCE_DIR}/renderervulkan/tr_scene.c
    ${SOURCE_DIR}/renderervulkan/tr_bsp.c
    ${SOURCE_DIR}/renderervulkan/tr_curve.c
    ${SOURCE_DIR}/renderervulkan/tr_light.c
    ${SOURCE_DIR}/renderervulkan/tr_shade_calc.c
    ${SOURCE_DIR}/renderervulkan/tr_marks.c
    ${SOURCE_DIR}/renderervulkan/tr_sprite.c
    ${SOURCE_DIR}/renderervulkan/tr_model.cpp
    ${SOURCE_DIR}/renderervulkan/tr_staticmodels.cpp
    ${SOURCE_DIR}/renderervulkan/tr_sky_portal.cpp
    ${SOURCE_DIR}/renderervulkan/tr_world.c
    ${SOURCE_DIR}/renderervulkan/tr_surface.c
    ${SOURCE_DIR}/renderervulkan/tr_main.c
    ${SOURCE_DIR}/renderervulkan/tr_sky.c
    ${SOURCE_DIR}/renderervulkan/tr_shade.c
    ${SOURCE_DIR}/renderervulkan/tr_shadows.c
    ${SOURCE_DIR}/renderervulkan/tr_sphere_shade.cpp
    ${SOURCE_DIR}/renderervulkan/tr_sun_flare.cpp
    ${SOURCE_DIR}/renderervulkan/tr_swipe.cpp
    ${SOURCE_DIR}/renderervulkan/tr_ghost.cpp
    ${SOURCE_DIR}/renderervulkan/vk_shade.c
    ${SOURCE_DIR}/renderervulkan/vk_vbo.c
    ${SOURCE_DIR}/renderervulkan/vk_probeplumb.c
    ${SOURCE_DIR}/renderervulkan/tr_util.cpp
    # the Vulkan substrate
    ${SOURCE_DIR}/renderervulkan/vk_procs.c
    ${SOURCE_DIR}/renderervulkan/vk_guard.c
    ${SOURCE_DIR}/renderervulkan/vk_instance.c
    ${SOURCE_DIR}/renderervulkan/vk_swapchain.c
    ${SOURCE_DIR}/renderervulkan/vk_memory.c
    ${SOURCE_DIR}/renderervulkan/vk_target.c
    ${SOURCE_DIR}/renderervulkan/vk_frame.c
    ${SOURCE_DIR}/renderervulkan/vk_offscreen.c
    ${SOURCE_DIR}/renderervulkan/vk_texture.c
    ${SOURCE_DIR}/renderervulkan/vk_draw.c
    ${SOURCE_DIR}/renderervulkan/vk_pipeline.c
    ${SOURCE_DIR}/renderervulkan/vk_vertex.c
    ${SOURCE_DIR}/renderervulkan/vk_descriptor.c
    ${SOURCE_DIR}/renderervulkan/vk_pipelinecache.c
    ${SOURCE_DIR}/renderervulkan/vk_firstlight.c
    ${SOURCE_DIR}/renderervulkan/vk_renderthread.c
    ${SOURCE_DIR}/renderervulkan/vk_platform.c
    ${SOURCE_DIR}/renderervulkan/vk_screenshot.c
)

set(RENDERER_VULKAN_BASENAME renderer_vulkan)
set(RENDERER_VULKAN_BINARY ${RENDERER_VULKAN_BASENAME})

# The shade layer's SPIR-V is compiled offline and embedded, so the build has
# to refuse a module whose embedded blobs were compiled from other shader
# sources: tools/build_vk_spirv.py --check compares the digest the header
# records against the sources on disk and fails the build on drift.  No
# compiler is needed to run it, so it gates every build, desktop and Android.
find_program(PYTHON3_EXECUTABLE NAMES python3)
# this file sits at <root>/app/src/main/cpp, the tool at <root>/tools
set(VK_SPIRV_TOOL ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tools/build_vk_spirv.py)
get_filename_component(VK_SPIRV_TOOL "${VK_SPIRV_TOOL}" ABSOLUTE)

list(APPEND RENDERER_VULKAN_BINARY_SOURCES
    ${RENDERER_COMMON_SOURCES}
    ${RENDERER_VULKAN_SOURCES}
    ${RENDERER_LIBRARY_SOURCES})

if(USE_RENDERER_DLOPEN)
    list(APPEND RENDERER_VULKAN_BINARY_SOURCES ${DYNAMIC_RENDERER_SOURCES})

    add_library(${RENDERER_VULKAN_BINARY} SHARED ${RENDERER_VULKAN_BINARY_SOURCES})

    target_link_libraries(      ${RENDERER_VULKAN_BINARY} PRIVATE ${RENDERER_LIBRARIES})
    target_include_directories( ${RENDERER_VULKAN_BINARY} PRIVATE ${RENDERER_INCLUDE_DIRS})
    target_compile_definitions( ${RENDERER_VULKAN_BINARY} PRIVATE
                                ${RENDERER_DEFINITIONS}
                                EF2_RENDERER_VULKAN=1
                                TIKI_BONE_ARENAS=2)
    target_compile_options(     ${RENDERER_VULKAN_BINARY} PRIVATE
                                ${RENDERER_COMPILE_OPTIONS}
                                -fno-strict-aliasing)
    target_link_options(        ${RENDERER_VULKAN_BINARY} PRIVATE ${RENDERER_LINK_OPTIONS})

    set_output_dirs(${RENDERER_VULKAN_BINARY})

    if(PYTHON3_EXECUTABLE)
        add_custom_target(vk_spirv_check
            COMMAND ${PYTHON3_EXECUTABLE} ${VK_SPIRV_TOOL} --check
            COMMENT "vk_spirv: the embedded SPIR-V against the shade sources"
            VERBATIM)
        add_dependencies(${RENDERER_VULKAN_BINARY} vk_spirv_check)
    else()
        message(STATUS "python3 not found: the embedded SPIR-V drift gate cannot run")
    endif()

    INSTALL(TARGETS ${RENDERER_VULKAN_BINARY} DESTINATION ${INSTALL_LIBDIR_FULL})

    if(MSVC)
        INSTALL(FILES $<TARGET_PDB_FILE:${RENDERER_VULKAN_BINARY}> DESTINATION ${INSTALL_LIBDIR_FULL} OPTIONAL)
    endif()
endif()
