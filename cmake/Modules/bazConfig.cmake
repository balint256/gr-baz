INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_BAZ baz)

FIND_PATH(
    BAZ_INCLUDE_DIRS
    NAMES baz/api.h
    HINTS $ENV{BAZ_DIR}/include
        ${PC_BAZ_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    BAZ_LIBRARIES
    NAMES gnuradio-baz
    HINTS $ENV{BAZ_DIR}/lib
        ${PC_BAZ_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
          )

include("${CMAKE_CURRENT_LIST_DIR}/bazTarget.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(BAZ DEFAULT_MSG BAZ_LIBRARIES BAZ_INCLUDE_DIRS)
MARK_AS_ADVANCED(BAZ_LIBRARIES BAZ_INCLUDE_DIRS)
