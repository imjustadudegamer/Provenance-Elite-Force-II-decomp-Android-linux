set(PROJECT_NAME provenanceef2)

# THE PRODUCT VERSION, FROM THE TREE'S ONE SOURCE OF TRUTH.
#
# This was `set(PROJECT_VERSION "")` -- an EMPTY version -- and `project(${PROJECT_NAME}
# VERSION ${PROJECT_VERSION} ...)` at CMakeLists.txt:12 therefore expanded the VERSION
# keyword to nothing.  CMake warned about it ("VERSION keyword not followed by a value
# or was followed by a value that expanded to nothing"), and the damage was not only the
# warning: version.cmake passes PROJECT_VERSION_MAJOR/MINOR/PATCH into
# q_version.generated.h, and common.c:1940 prints PRODUCT_VERSION_FULL in the startup
# banner -- which is why the banner read "ProvenanceEF2  linux-arm64-debug ..." with a
# gap where the version belongs.
#
# app/src/main/cpp/VERSION is read here AND by app/build.gradle (for versionName),
# so the engine banner and the shipped APK cannot drift apart.  Bump that file, not this.
file(READ "${CMAKE_CURRENT_LIST_DIR}/../VERSION" PROJECT_VERSION)
string(STRIP "${PROJECT_VERSION}" PROJECT_VERSION)

if(NOT PROJECT_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
	message(FATAL_ERROR "app/src/main/cpp/VERSION must hold a MAJOR.MINOR.PATCH version, got '${PROJECT_VERSION}'")
endif()

set(SERVER_NAME provenanceef2ded)
set(CLIENT_NAME provenanceef2)

set(BASEGAME)

set(CGAME_MODULE cgame)
set(GAME_MODULE game)
set(UI_MODULE ui)

set(WINDOWS_ICON_PATH ${CMAKE_SOURCE_DIR}/misc/windows/provenanceef2.ico)

set(MACOS_ICON_PATH ${CMAKE_SOURCE_DIR}/misc/macos/provenanceef2.icns)
set(MACOS_BUNDLE_ID org.provenanceef2.${CLIENT_NAME})

set(COPYRIGHT "Unofficial Star Trek: Elite Force II port. Based on OpenMoHAA (© OpenMoHAA Team).")

set(CONTACT_EMAIL "")
set(PROTOCOL_HANDLER_SCHEME provenanceef2)
