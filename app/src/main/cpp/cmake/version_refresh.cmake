# cmake/version_refresh.cmake -- the version stamp, read from git at build time.
#
# This file is part of the port of Star Trek: Voyager -- Elite Force II, whose
# engine descends from id Tech 3 through OpenMoHAA.
#
# It is free software; you can redistribute it and/or modify it under the terms
# of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# It is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE.  See the GNU General Public License, COPYING.txt at the
# repository root, for more details.
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(GIT_DIRTY "")

execute_process(
	COMMAND git rev-parse --is-inside-work-tree
	WORKING_DIRECTORY ${SRC_DIR}
	OUTPUT_VARIABLE GIT_IS_INSIDE_WORK
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)

if ("${GIT_IS_INSIDE_WORK}" STREQUAL "true")
	execute_process(
		COMMAND git branch --show-current
		WORKING_DIRECTORY ${SRC_DIR}
		OUTPUT_VARIABLE GIT_BRANCH_NAME
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
	)
	execute_process(
		COMMAND git show -s --format=%H HEAD
		WORKING_DIRECTORY ${SRC_DIR}
		OUTPUT_VARIABLE GIT_REVISION_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
	)
	execute_process(
		COMMAND git show -s --format=%h HEAD
		WORKING_DIRECTORY ${SRC_DIR}
		OUTPUT_VARIABLE GIT_REVISION_HASH_ABBREVIATED
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
	)
	execute_process(
		COMMAND git status --porcelain
		WORKING_DIRECTORY ${SRC_DIR}
		OUTPUT_VARIABLE GIT_DIRTY_OUT
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
	)
	if (NOT "${GIT_DIRTY_OUT}" STREQUAL "")
		set(GIT_DIRTY "-dirty")
	endif()
endif()

if (NOT GIT_REVISION_HASH_ABBREVIATED)
	set(GIT_REVISION_HASH_ABBREVIATED "nogit")
endif()
if (NOT GIT_REVISION_HASH)
	set(GIT_REVISION_HASH "nogit")
endif()
if (NOT GIT_BRANCH_NAME)
	set(GIT_BRANCH_NAME "unknown")
endif()

string(TIMESTAMP GIT_REVISION_DATE_TIME UTC)
string(TIMESTAMP GIT_REVISION_DATE "%b %d %Y UTC" UTC)

if (NOT BUILD_NUMBER)
	set(BUILD_NUMBER 0)
endif()
if (NOT STAGE)
	set(STAGE "unstable")
endif()

set(OUT "${OUT_DIR}/generated/q_version.generated.h")
set(TMP "${OUT_DIR}/generated/q_version.generated.h.tmp")

file(WRITE ${TMP} "/*
===========================================================================
Copyright (C) 2023 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// q_version.generated.h -- file versioning, refreshed at build time

#define GIT_BRANCH_NAME \"${GIT_BRANCH_NAME}\"
#define GIT_REVISION_HASH \"${GIT_REVISION_HASH}\"
#define GIT_REVISION_HASH_ABBREVIATED \"${GIT_REVISION_HASH_ABBREVIATED}\"
#define GIT_REVISION_DATE_TIME \"${GIT_REVISION_DATE_TIME}\"
#define GIT_REVISION_DATE \"${GIT_REVISION_DATE}\"
#define GIT_REVISION_DIRTY \"${GIT_DIRTY}\"
#define GIT_REVISION_BUILD_NUMBER ${BUILD_NUMBER}

#define PRODUCT_VERSION_MAJOR ${VERSION_MAJOR}
#define PRODUCT_VERSION_MINOR ${VERSION_MINOR}
#define PRODUCT_VERSION_PATCH ${VERSION_PATCH}
#define PRODUCT_VERSION_STAGE \"${STAGE}\"
")

if (NOT EXISTS ${OUT})
	file(RENAME ${TMP} ${OUT})
	message(STATUS "version_refresh: wrote ${OUT} (${GIT_REVISION_HASH_ABBREVIATED}${GIT_DIRTY})")
else()
	file(READ ${OUT} OLD_CONTENT)
	file(READ ${TMP} NEW_CONTENT)
	if (NOT "${OLD_CONTENT}" STREQUAL "${NEW_CONTENT}")
		file(RENAME ${TMP} ${OUT})
		message(STATUS "version_refresh: refreshed to ${GIT_REVISION_HASH_ABBREVIATED}${GIT_DIRTY}")
	else()
		file(REMOVE ${TMP})
	endif()
endif()
