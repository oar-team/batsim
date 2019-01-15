# Try to find docopt and validate that it is installed as it should be
# Once done it will define
# - DOCOPT_FOUND
# - DOCOPT_INCLUDE_DIRS
# - DOCOPT_LIBRARIES

# We won't be using PkgConfig and maybe this should change in the future,
# all we are about to do is check if we can find the file <docopt/docopt.h> and
# the libdocopt library (static or shared we don't care)

find_path(DOCOPT_INCLUDE_DIRS docopt/docopt.h)
find_library(DOCOPT_LIBRARIES docopt)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    docopt
    DEFAULT_MSG
    DOCOPT_INCLUDE_DIRS
    DOCOPT_LIBRARIES
)
