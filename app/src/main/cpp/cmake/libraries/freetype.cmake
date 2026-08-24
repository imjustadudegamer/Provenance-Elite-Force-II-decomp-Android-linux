if(NOT USE_FREETYPE)
    return()
endif()

# same rule as jpeg.cmake -- every renderer compiles the font path too.
if(NOT BUILD_RENDERER_GLES3 AND NOT BUILD_CLIENT)
    return()
endif()

find_package(Freetype REQUIRED)

list(APPEND RENDERER_INCLUDE_DIRS ${FREETYPE_INCLUDE_DIRS})
list(APPEND RENDERER_LIBRARIES ${FREETYPE_LIBRARIES})
