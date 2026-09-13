execute_process(
	COMMAND git rev-parse --is-inside-work-tree
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	OUTPUT_VARIABLE GIT_IS_INSIDE_WORK
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

if ("${GIT_IS_INSIDE_WORK}" STREQUAL "true")
	message(VERBOSE "Git was successfully detected in the source directory. Relevant Git information will be included in the game version.")

	# Current branch
	execute_process(
		COMMAND git branch --show-current
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_BRANCH_NAME
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	# Revision
	execute_process(
		COMMAND git show -s --format=%H HEAD
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_REVISION_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	# Revision
	execute_process(
		COMMAND git show -s --format=%h HEAD
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_REVISION_HASH_ABBREVIATED
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	# Commit date
	execute_process(
		COMMAND git show -s --format=%ct
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_REVISION_DATE
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
else()
	message(WARNING "The repository was not detected as a Git working directory. Git information will not be available in the game version number.")
endif()

# A build that carries uncommitted work must say so: two binaries from the same
# commit with different working trees are not the same binary.
if ("${GIT_IS_INSIDE_WORK}" STREQUAL "true")
	execute_process(
		COMMAND git status --porcelain
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_DIRTY_OUT
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	if (NOT "${GIT_DIRTY_OUT}" STREQUAL "")
		set(GIT_REVISION_DIRTY "-dirty")
	else()
		set(GIT_REVISION_DIRTY "")
	endif()
else()
	set(GIT_REVISION_DIRTY "")
endif()

# The header is valid whatever git could or could not report, so the binary
# always has a label and never shells out at run time.
if (NOT GIT_REVISION_HASH_ABBREVIATED)
	set(GIT_REVISION_HASH_ABBREVIATED "nogit")
endif()

if (NOT GIT_REVISION_HASH)
	set(GIT_REVISION_HASH "nogit")
endif()

string(TIMESTAMP GIT_REVISION_DATE_TIME UTC)

set(GIT_REVISION_DATE ${GIT_REVISION_DATE_TIME})
string(TIMESTAMP GIT_REVISION_DATE "%b %d %Y UTC" UTC)

if(NOT GIT_REVISION_BUILD_NUMBER)
	set(GIT_REVISION_BUILD_NUMBER 0)
endif()

if(NOT PRODUCT_VERSION_STAGE)
	set(PRODUCT_VERSION_STAGE "unstable")
endif()

message(VERBOSE "Git branch: ${GIT_BRANCH_NAME}")
message(VERBOSE "Git revision hash: ${GIT_REVISION_HASH}")
message(VERBOSE "Git revision date: ${GIT_REVISION_DATE}")
message(VERBOSE "Git revision build: ${GIT_REVISION_BUILD_NUMBER}")
message(VERBOSE "Stage: ${PRODUCT_VERSION_STAGE}")

configure_file("cmake/q_version.generated.h.in" "${CMAKE_CURRENT_BINARY_DIR}/generated/q_version.generated.h")

list(APPEND CLIENT_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/generated")
list(APPEND SERVER_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/generated")
list(APPEND RENDERER_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/generated")

# The stamp is a property of the tree at build time, not of the day cmake ran:
# a commit made after the configure step must not ship under the previous
# commit's name.  version_refresh re-reads git when ninja runs and rewrites the
# header only when the label would change, so an unchanged tree costs nothing
# and a moved HEAD recompiles exactly the objects that include it.
set(VERSION_REFRESH_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version_refresh.cmake")
add_custom_target(version_refresh ALL
	COMMAND ${CMAKE_COMMAND}
		-DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}
		-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
		-DSTAGE=${PRODUCT_VERSION_STAGE}
		-DBUILD_NUMBER=${GIT_REVISION_BUILD_NUMBER}
		-DVERSION_MAJOR=${PROJECT_VERSION_MAJOR}
		-DVERSION_MINOR=${PROJECT_VERSION_MINOR}
		-DVERSION_PATCH=${PROJECT_VERSION_PATCH}
		-P ${VERSION_REFRESH_SCRIPT}
	BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/generated/q_version.generated.h"
	COMMENT "version_refresh: the stamp is read from git at build time"
	VERBATIM
)
