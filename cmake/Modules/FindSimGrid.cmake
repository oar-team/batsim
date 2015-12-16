# Try fo find Simgrid:
#   SIMGRID_FOUND
#   SIMGRID_INCLUDE_DIRS
#   SIMGRID_LIBRARIES
#   SIMGRID_DEFINITIONS

message("-- Looking for simgrid.h")
find_path(SIMGRID_INCLUDE_DIR
          NAMES simgrid.h
          HINTS ENV SIMGRID_PATH ENV INCLUDE ENV CPATH
          PATHS /opt/simgrid /opt/Simgrid
          PATH_SUFFIXES include)
message("-- Looking for simgrid.h - ${SIMGRID_INCLUDE_DIR}")

message("-- Looking for libsimgrid")
find_library(SIMGRID_LIBRARY
             NAMES simgrid
             HINTS ENV SIMGRID_PATH ENV LIBRARY_PATH
             PATHS /opt/simgrid /opt/Simgrid
             PATH_SUFFIXES lib)
message("-- Looking for libsimgrid -- ${SIMGRID_LIBRARY}")

set(SIMGRID_LIBRARIES ${SIMGRID_LIBRARY} )
set(SIMGRID_INCLUDE_DIRS ${SIMGRID_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SimGrid DEFAULT_MSG
                                  SIMGRID_LIBRARY SIMGRID_INCLUDE_DIR)
mark_as_advanced(SIMGRID_INCLUDE_DIR SIMGRID_LIBRARY)