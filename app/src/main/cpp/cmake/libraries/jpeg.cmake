# "the fixed-function renderer is the only consumer" stopped being true when a
# second backend grew its own image path -- every renderer compiles the same
# renderercommon/tr_image_jpg.c out of RENDERER_COMMON_SOURCES, and with the
# one remaining renderer deselected the internal jpeg sources were never added
# to RENDERER_LIBRARY_SOURCES, leaving 15 undefined jpeg_* symbols in the
# renderer module.  Gate on "some renderer is being built" instead.
if(NOT BUILD_RENDERER_GLES3 AND NOT BUILD_CLIENT)
    return()
endif()

include(utils/disable_warnings)
include(utils/find_include_dirs)

set(INTERNAL_JPEG_DIR ${SOURCE_DIR}/thirdparty/jpeg-9f)

if(USE_INTERNAL_JPEG)
    file(GLOB_RECURSE JPEG_SOURCES ${INTERNAL_JPEG_DIR}/j*.c)
    disable_warnings(${JPEG_SOURCES})
    find_include_dirs(JPEG_INCLUDE_DIRS ${INTERNAL_JPEG_DIR})
    set(JPEG_DEFINITIONS USE_INTERNAL_JPEG)
    list(APPEND RENDERER_LIBRARY_SOURCES ${JPEG_SOURCES})
else()
    find_package(JPEG REQUIRED)
endif()

list(APPEND RENDERER_LIBRARIES ${JPEG_LIBRARIES})
list(APPEND RENDERER_INCLUDE_DIRS ${JPEG_INCLUDE_DIRS})
list(APPEND RENDERER_DEFINITIONS ${JPEG_DEFINTIONS})
