# Try fo find Redox.
# Once done, this will define:
#   REDOX_FOUND
#   REDOX_INCLUDE_DIRS
#   REDOX_LIBRARIES

find_path(
    REDOX_INCLUDE_DIR
    NAMES redox.hpp
    HINTS ${REDOX_ROOT_DIR})

find_library(
    REDOX_LIBRARY
    NAMES redox
    HINTS ${REDOX_ROOT_DIR}
    PATH_SUFFIXES ${CMAKE_INSTALL_LIBDIR})

set(REDOX_INCLUDE_DIRS ${REDOX_INCLUDE_DIR})
set(REDOX_LIBRARIES ${REDOX_LIBRARY})

include (FindPackageHandleStandardArgs)
    find_package_handle_standard_args(
    redox DEFAULT_MSG REDOX_LIBRARY REDOX_INCLUDE_DIR)

mark_as_advanced(REDOX_LIBRARY REDOX_INCLUDE_DIR)
