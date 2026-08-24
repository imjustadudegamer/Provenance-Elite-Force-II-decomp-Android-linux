# Linux-like specific settings, i.e. including FreeBSD etc.

# CMake reports UNIX for Android too (it is Linux underneath); the desktop
# packaging/icon settings below are host-only and would leak a USE_ICON define
# and a DEB generator into the device build, so Android takes platforms/android.
if(ANDROID)
    return()
endif()

if(NOT UNIX)
    return()
endif()

list(APPEND CLIENT_DEFINITIONS USE_ICON)
list(APPEND RENDERER_DEFINITIONS USE_ICON)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE /opt/mohaa)
endif()

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
