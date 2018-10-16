# Let's use at least v3.1 for sanity's sake.
cmake_minimum_required(VERSION 3.1)

project("Batsim")

# Enable C++11
# Let's check that the used compiler handles c++11 correctly
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.8.1")
        message(FATAL_ERROR "Insufficient gcc version: 4.8.1 is needed to support C++11")
    elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5")
        message(WARNING "Old gcc version found: Using version 5 or greater is recommended")
    endif()
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "3.3")
        message(FATAL_ERROR "Insufficient clang version: 3.3 is needed to support C++11")
    elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "3.4")
        message(WARNING "Old clang version found: Using version 3.4 or greater is recommended")
    endif()
else()
    message(WARNING "Unknown compiler. Make sure it fully supports C++11.")
endif()

# Let's enable C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
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
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    "${CMAKE_SOURCE_DIR}/cmake/Modules/")

## SimGrid dependency
find_package(SimGrid REQUIRED)
include_directories(BEFORE SYSTEM ${SimGrid_INCLUDE_DIR})

## Boost dependency
find_package(Boost 1.58 REQUIRED COMPONENTS system filesystem regex locale)
include_directories(${Boost_INCLUDE_DIR})

## GMP
find_package(GMP REQUIRED)
include_directories(${GMP_INCLUDE_DIR})

## Rapidjson dependency
find_package(rapidjson REQUIRED)
include_directories(${RAPIDJSON_INCLUDE_DIRS})

## OpenSSL dependency
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

## Redox dependency
find_package(redox REQUIRED)
include_directories(${REDOX_INCLUDE_DIR})

## Redox sub dependencies
find_package(hiredis REQUIRED)
include_directories(${HIREDIS_INCLUDE_DIRS})

find_package(libev REQUIRED)
include_directories(${LIBEV_INCLUDE_DIRS})

## ZeroMQ dependency
find_package(ZMQ REQUIRED)
include_directories(${ZMQ_INCLUDE_DIRS})

## Docopt dependency
find_package(docopt REQUIRED)
include_directories(${DOCOPT_INCLUDE_DIRS})

## Pugixml dependency
find_package(pugixml REQUIRED)
include_directories(${PUGIXML_INCLUDE_DIR})

## Intervalset dependency
find_package(intervalset REQUIRED)
include_directories(${INTERVALSET_INCLUDE_DIR})

##################
# Batsim version #
##################
set(default_batsim_version "v2.0.0")
include(GetGitRevisionDescription)
git_describe(batsim_version)
message(STATUS "Batsim version from git: ${batsim_version}")

if(NOT((${batsim_version} STREQUAL "GIT-NOTFOUND") OR
       (${batsim_version} STREQUAL "HEAD-HASH-NOTFOUND")))
    add_definitions(-DBATSIM_VERSION=${batsim_version})
else()
    message(WARNING "Cannot retrieve Batsim version from git. "
                    "Using default version ${default_batsim_version}")
    add_definitions(-DBATSIM_VERSION=${default_batsim_version})
endif()

################
# Source files #
################

## Batsim
file(GLOB batsim_SRC
    "src/*.hpp"
    "src/*.cpp"
    "src/pugixml-1.7/*.hpp"
    "src/pugixml-1.7/*.cpp"
    "src/docopt/*.cpp"
    "src/docopt/*.h"
    "src/unittest/*.hpp"
    "src/unittest/*.cpp")

# Executables
add_executable(batsim ${batsim_SRC})

# Libraries to link
target_link_libraries(batsim
    ${SimGrid_LIBRARY}
    ${GMP_LIBRARIES}
    ${Boost_FILESYSTEM_LIBRARY_DEBUG}
    ${Boost_LOCALE_LIBRARY_DEBUG}
    ${Boost_REGEX_LIBRARY_DEBUG}
    ${Boost_SYSTEM_LIBRARY_DEBUG}
    ${OPENSSL_LIBRARIES}
    ${REDOX_LIBRARY}
    ${LIBEV_LIBRARY}
    ${HIREDIS_LIBRARY}
    ${ZMQ_LIBRARIES}
    ${DOCOPT_LIBRARIES}
    ${PUGIXML_LIBRARY}
    ${INTERVALSET_LIBRARY}
)

################
# Installation #
################
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/batsim
    DESTINATION bin)

# Let's enable warnings if needed
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

# Let's ignore assertions if needed (might improve performance but can be dangerous)
if (ignore_assertions)
    target_compile_definitions(batsim PRIVATE NDEBUG)
endif()

###########
# Testing #
###########

# Batexpe test dependency
include("${CMAKE_SOURCE_DIR}/cmake/Modules/batexpe.cmake")

if (${robin_COMMAND} STREQUAL "robin_COMMAND-NOTFOUND")
    message(WARNING "Could not find robin command. Tests are not set up.")
elseif(${robintest_COMMAND} STREQUAL "robintest_COMMAND-NOTFOUND")
    message(WARNING "Could not find robintest command. Tests are not set up.")
else()
    enable_testing()
    add_subdirectory(test)
endif()
