# Use at least v3.1 for sanity's sake.
cmake_minimum_required(VERSION 3.1)

project("Batsim")

# Check that the used compiler handles c++17 correctly
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "8")
        message(FATAL_ERROR "Insufficient gcc version: 8 is needed to support C++17")
    endif()
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5")
        message(FATAL_ERROR "Insufficient clang version: 5 is needed to support C++17")
    endif()
else()
    message(WARNING "Unknown compiler. Make sure it fully supports C++17.")
endif()

# Enable C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(enable_compile_optimizations "Enable compile optimizations" ON)
option(enable_warnings "Enable compilation warnings" ON)
option(treat_warnings_as_errors "Treat compilation warnings as compilation errors" OFF)
option(ignore_assertions "Ignore assertions, which could make the simulation unstable, but could improve its performance" OFF)

# Build type
# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Debug' as none was specified.")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()

################
# Dependencies #
################
find_package(PkgConfig REQUIRED)

pkg_check_modules(simgrid REQUIRED IMPORTED_TARGET simgrid)
pkg_check_modules(rapidjson REQUIRED IMPORTED_TARGET RapidJSON)
pkg_check_modules(libzmq REQUIRED IMPORTED_TARGET libzmq)
pkg_check_modules(pugixml REQUIRED IMPORTED_TARGET pugixml)
pkg_check_modules(intervalset REQUIRED IMPORTED_TARGET intervalset)
pkg_check_modules(batprotocol-cpp REQUIRED IMPORTED_TARGET batprotocol-cpp)
pkg_check_modules(cli11 REQUIRED IMPORTED_TARGET CLI11)

# (boost does not provide pkgconfig files)
find_package(Boost 1.58)
include_directories(${Boost_INCLUDE_DIR})

##################
# Batsim version #
##################
message(WARNING "CMake support is deprecated, please use Meson instead.")
add_definitions(-DBATSIM_VERSION="batsim-build-from-cmake.this-is-deprecated.use-meson-instead")

################
# Source files #
################
## Batsim
file(GLOB batsim_SRC
    "src/*.hpp"
    "src/*.cpp"
)

# Executables
add_executable(batsim ${batsim_SRC})

# Libraries to link
target_link_libraries(batsim
    ${simgrid_LIBRARIES}
    ${libzmq_LIBRARIES}
    ${pugixml_LIBRARIES}
    ${intervalset_LIBRARIES}
    ${batprotocol-cpp_LIBRARIES}
    "'stdc++fs'"
    ${CMAKE_DL_LIBS} # dlmopen and friends
)

include_directories(
    ${simgrid_INCLUDE_DIRS}
    ${rapidjson_INCLUDE_DIRS}
    ${libzmq_INCLUDE_DIRS}
    ${pugixml_INCLUDE_DIRS}
    ${intervalset_INCLUDE_DIRS}
    ${batprotocol-cpp_INCLUDE_DIRS}
    ${cli11_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIR}
)

################
# Installation #
################
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/batsim DESTINATION bin)

# Enable or disable optimizations depending on user's will
message("enable_compile_optimizations is ${enable_compile_optimizations}")
if (enable_compile_optimizations)
    set(compile_optization_flag " -O2")
else()
    set(compile_optization_flag " -O0")
endif()
set_property(TARGET batsim
    APPEND_STRING PROPERTY COMPILE_FLAGS ${compile_optization_flag})

# Enable warnings if needed
message("enable_warnings is ${enable_warnings}")
if (enable_warnings)
    set(warning_flags " -Wall -Wextra")

    if (treat_warnings_as_errors)
        set(warning_flags "${warning_flags} -Werror")
    endif()

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set_property(TARGET batsim APPEND_STRING PROPERTY COMPILE_FLAGS ${warning_flags})
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set_property(TARGET batsim APPEND_STRING PROPERTY COMPILE_FLAGS ${warning_flags})
    else()
        message(WARNING "Unknown compiler. Warnings should not be enabled correctly.")
        set_property(TARGET batsim APPEND_STRING PROPERTY COMPILE_FLAGS ${warning_flags})
    endif()
endif()

# Ignore assertions if needed (might improve performance but can be dangerous)
if (ignore_assertions)
    target_compile_definitions(batsim PRIVATE NDEBUG)
endif()
