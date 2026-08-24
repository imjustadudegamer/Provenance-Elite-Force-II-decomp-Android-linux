include_directories(${SOURCE_DIR}/gamespy/common)
include_directories(${SOURCE_DIR}/gamespy/gcdkey)

file(GLOB SRCS_common
	${SOURCE_DIR}/gamespy/common/*.c
    ${SOURCE_DIR}/gamespy/darray.c
    ${SOURCE_DIR}/gamespy/gserver.c
    ${SOURCE_DIR}/gamespy/gserverlist.c
	${SOURCE_DIR}/gamespy/hashtable.c
	${SOURCE_DIR}/gamespy/md5c.c
	${SOURCE_DIR}/gamespy/gutil.c
)

file(GLOB SRCS_gcdkey
    ${SOURCE_DIR}/gamespy/gcdkey/*.c
)

file(GLOB SRCS_natneg
    ${SOURCE_DIR}/gamespy/natneg/*.c
)

file(GLOB SRCS_qr2
    ${SOURCE_DIR}/gamespy/qr2/*.c
)

add_library(gcd_common STATIC ${SRCS_common})
add_library(gcd_key STATIC ${SRCS_gcdkey})
add_library(gcd_natneg STATIC ${SRCS_natneg})
add_library(gcd_qr2 STATIC ${SRCS_qr2})
add_library(gcd INTERFACE)

set_property(TARGET gcd_common gcd_key PROPERTY POSITION_INDEPENDENT_CODE ON)

#
# Warning suppressions for the vendored GameSpy SDK.
#
# Everything compiled into the four gcd_* libraries is unmodified GameSpy SDK
# source, kept that way so it stays re-syncable. Its warnings are silenced at
# the build level instead of by editing the vendor sources.
#
# Every flag below is attached PRIVATE, so it lands on the COMPILE_OPTIONS of
# these four static libraries and on nothing else. PRIVATE never populates
# INTERFACE_COMPILE_OPTIONS, so it cannot travel across target_link_libraries()
# into gcd, provenanceef2, provenanceef2ded or any of the engine's own targets.
#
# The engine-side glue that also lives in code/gamespy (cl_gamespy.c,
# q_gamespy.c) is NOT covered here: it is compiled into the engine executables,
# so its warnings were fixed in the source instead.  The two MOHAA server
# responder TUs (sv_gamespy.c, sv_gqueryreporting.c) are compiled by nothing --
# EF2 retail has no gamespy query responder in the engine -- and sv_gamespy.c
# was deleted with the multi-game targeting layer it indexed on com_target_game.
#
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "^(Apple)?Clang$")
	include(CheckCCompilerFlag)

	# Probe the positive spelling of each warning: GCC accepts any unknown
	# -Wno-* on the command line without complaint, so probing "-Wno-foo"
	# would always succeed and tell us nothing.
	set(GAMESPY_VENDOR_NOWARN "")
	foreach(GAMESPY_WARNING
		strict-prototypes       # K&R-era "()" declarations all over the SDK
		pointer-sign            # int*/socklen_t* and char*/uchar* at the socket calls
		shadow
		unused-variable
		unused-function
		unused-but-set-variable
		misleading-indentation  # the SDK's base64 encoder is indented misleadingly, but is correct
		int-to-pointer-cast     # the SDK's "percent done in the callback's void* param" idiom
	)
		string(MAKE_C_IDENTIFIER "GAMESPY_HAS_W_${GAMESPY_WARNING}" GAMESPY_WARNING_VAR)
		check_c_compiler_flag("-W${GAMESPY_WARNING}" ${GAMESPY_WARNING_VAR})
		if(${GAMESPY_WARNING_VAR})
			list(APPEND GAMESPY_VENDOR_NOWARN "-Wno-${GAMESPY_WARNING}")
		endif()
	endforeach()

	foreach(GAMESPY_LIB gcd_common gcd_key gcd_natneg gcd_qr2)
		target_compile_options(${GAMESPY_LIB} PRIVATE ${GAMESPY_VENDOR_NOWARN})
	endforeach()
endif()

if(UNIX)
	add_definitions(-D_LINUX=1)
endif()

set(DEPENDENT_LIBS
	gcd_key
#	gcd_gp
#	gcd_gstats
	gcd_natneg
#	gcd_pinger
#	gcd_pt
	gcd_qr2
#	gcd_sake
#	gcd_sc
#	gcd_serverbrowsing
#	gcd_webservices
)

target_link_libraries(gcd INTERFACE ${DEPENDENT_LIBS})

foreach(LIB ${DEPENDENT_LIBS})
	target_link_libraries(${LIB} PUBLIC gcd_common)
endforeach()

target_link_libraries(gcd_key PRIVATE gcd_qr2)
target_link_libraries(gcd_qr2 PRIVATE gcd_natneg)

list(APPEND CLIENT_LIBRARIES gcd)
list(APPEND SERVER_LIBRARIES gcd)
