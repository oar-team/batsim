# Find the intervalset library (https://framagit.org/batsim/intervalset)
#
# Sets the usual variables expected for find_package scripts:
#
# INTERVALSET_INCLUDE_DIR - header location
# INTERVALSET_LIBRARIES - library to link against
# INTERVALSET_FOUND - true if intervalset was found.

find_path(INTERVALSET_INCLUDE_DIR intervalset.hpp)
find_library(INTERVALSET_LIBRARY intervalset)

# Support the REQUIRED and QUIET arguments, and set INTERVALSET_FOUND if found.
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(intervalset DEFAULT_MSG INTERVALSET_LIBRARY
                                  INTERVALSET_INCLUDE_DIR)

mark_as_advanced(INTERVALSET_LIBRARY INTERVALSET_INCLUDE_DIR)
