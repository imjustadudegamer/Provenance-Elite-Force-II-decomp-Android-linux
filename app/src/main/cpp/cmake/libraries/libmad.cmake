if(NOT USE_CODEC_MAD)
    return()
endif()

if(NOT BUILD_CLIENT)
    return()
endif()

set(OLD_VALUE ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

if(USE_INTERNAL_MAD)
	add_subdirectory("${SOURCE_DIR}/thirdparty/libmad" "./libmad" EXCLUDE_FROM_ALL)

    #
    # Warning suppressions for the vendored libmad sources (upstream 0.16.4).
    #
    #  -Wshadow            layer3.c's main_data lookahead block declares a local
    #                      "unsigned long header" inside mad_layer_III(), which
    #                      already has a "struct mad_header *header".
    #  -Wstringop-overflow layer3.c's sdctII() calls fastsdct(tmp, &X[1]). The
    #                      callee's parameter is nominally mad_fixed_t[18] but it
    #                      only ever writes the even indices y[0]..y[16], which
    #                      stay inside X[18]. GCC takes the declared bound at
    #                      face value and reports a 72-vs-68 byte overrun.
    #
    # PRIVATE, so this stays on mad's own COMPILE_OPTIONS and is not inherited
    # by the client target that links it.
    #
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "^(Apple)?Clang$")
        include(CheckCCompilerFlag)
        # uivk: +format (minimad.c example's %x/long); uiparity: +format-security --
        # the project's own flags carry -Wformat-security, and with -Wno-format on
        # this target GCC/clang report that combination itself ("'-Wformat-security'
        # ignored without '-Wformat'") once per vendored TU.  Disabling it here
        # explicitly is the pairing the two flags need; upstream stays unmodified.
        foreach(LIBMAD_WARNING shadow stringop-overflow format format-security)
            string(MAKE_C_IDENTIFIER "LIBMAD_HAS_W_${LIBMAD_WARNING}" LIBMAD_WARNING_VAR)
            check_c_compiler_flag("-W${LIBMAD_WARNING}" ${LIBMAD_WARNING_VAR})
            if(${LIBMAD_WARNING_VAR})
                target_compile_options(mad PRIVATE "-Wno-${LIBMAD_WARNING}")
            endif()
        endforeach()

    # uivk: the minimad EXAMPLE binary ships an upstream %x/long mismatch; the
    # example is not shipped, silence instead of editing upstream.
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(mad_example PRIVATE -Wno-format)
    endif()
    endif()

    list(APPEND CLIENT_LIBRARIES mad)
else()
	include(FindPackageHandleStandardArgs)
	find_path(LIBMAD_INCLUDE_DIRS mad.h)
	find_library(LIBMAD_LIBRARIES mad)
	find_package_handle_standard_args(
		LibMad
		DEFAULT_MSG
		LIBMAD_LIBRARIES
		LIBMAD_INCLUDE_DIRS
	)

    list(APPEND CLIENT_INCLUDE_DIRS ${LIBMAD_INCLUDE_DIRS})
    list(APPEND CLIENT_LIBRARIES ${LIBMAD_LIBRARIES})
endif()

set(BUILD_SHARED_LIBS ${OLD_VALUE} CACHE BOOL "" FORCE)

list(APPEND CLIENT_DEFINITIONS USE_CODEC_MAD=1)
