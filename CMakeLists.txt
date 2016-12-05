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
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

## SimGrid dependency
find_package(SimGrid REQUIRED)
include_directories(${SIMGRID_INCLUDE_DIR})

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

################
# Source files #
################

## Batsim
file(GLOB batsim_SRC
    "src/*.hpp"
    "src/*.cpp"
    "src/pugixml-1.7/*.hpp"
    "src/pugixml-1.7/*.cpp")

# Executables
add_executable(batsim ${batsim_SRC})

# Libraries to link
target_link_libraries(batsim
                      ${HAVE_SIMGRID_LIB}
                      ${GMP_LIBRARIES}
                      ${Boost_FILESYSTEM_LIBRARY_DEBUG}
                      ${Boost_LOCALE_LIBRARY_DEBUG}
                      ${Boost_REGEX_LIBRARY_DEBUG}
                      ${Boost_SYSTEM_LIBRARY_DEBUG}
                      ${OPENSSL_LIBRARIES}
                      ${REDOX_LIBRARY}
                      ${LIBEV_LIBRARY}
                      ${HIREDIS_LIBRARY})

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
enable_testing()

# Execution scripts tests
add_test(exec1_tiny
         ${CMAKE_SOURCE_DIR}/tools/experiments/execute_one_instance.py
         ${CMAKE_SOURCE_DIR}/tools/experiments/instance_examples/pybatsim_filler_tiny.yaml
         -od /tmp/batsim_tools_tests/exec1/out/instance_examples/pftiny
         -wd ${CMAKE_SOURCE_DIR})

add_test(exec1_medium
         ${CMAKE_SOURCE_DIR}/tools/experiments/execute_one_instance.py
         ${CMAKE_SOURCE_DIR}/tools/experiments/instance_examples/pybatsim_filler_medium.yaml
         -od /tmp/batsim_tools_tests/exec1/out/instance_examples/pfmedium
         -wd ${CMAKE_SOURCE_DIR})

add_test(execN_mixed
         ${CMAKE_SOURCE_DIR}/tools/experiments/execute_instances.py
         ${CMAKE_SOURCE_DIR}/tools/experiments/instances_examples/pyfiller_tiny_medium_mixed.yaml
         -bod /tmp/batsim_tools_tests/execN
         -bwd ${CMAKE_SOURCE_DIR})

add_test(execN_ssh1
         ${CMAKE_SOURCE_DIR}/tools/experiments/execute_instances.py
         ${CMAKE_SOURCE_DIR}/test/test_no_energy.yaml
         -bwd ${CMAKE_SOURCE_DIR}
         -bod /tmp/batsim_tools_tests/ssh1
         --mpi_hostfile ${CMAKE_SOURCE_DIR}/test/mpi_hostfile_localhost
         --nb_workers_per_host 1)

add_test(execN_ssh2
         ${CMAKE_SOURCE_DIR}/tools/experiments/execute_instances.py
         ${CMAKE_SOURCE_DIR}/test/test_no_energy.yaml
         -bwd ${CMAKE_SOURCE_DIR}
         -bod /tmp/batsim_tools_tests/ssh2
         --mpi_hostfile ${CMAKE_SOURCE_DIR}/test/mpi_hostfile_localhost
         --nb_workers_per_host 2)

add_test(execN_ssh4
         ${CMAKE_SOURCE_DIR}/tools/experiments/execute_instances.py
         ${CMAKE_SOURCE_DIR}/test/test_no_energy.yaml
         -bwd ${CMAKE_SOURCE_DIR}
         -bod /tmp/batsim_tools_tests/ssh4
         --mpi_hostfile ${CMAKE_SOURCE_DIR}/test/mpi_hostfile_localhost
         --nb_workers_per_host 4)

# Batsim tests
add_test(no_energy
         ${CMAKE_SOURCE_DIR}/tools/experiments/execute_instances.py
         ${CMAKE_SOURCE_DIR}/test/test_no_energy.yaml
         -bwd ${CMAKE_SOURCE_DIR})

add_test(energy
         ${CMAKE_SOURCE_DIR}/tools/experiments/execute_instances.py
         ${CMAKE_SOURCE_DIR}/test/test_energy.yaml
         -bwd ${CMAKE_SOURCE_DIR})

add_test(walltime
         ${CMAKE_SOURCE_DIR}/tools/experiments/execute_instances.py
         ${CMAKE_SOURCE_DIR}/test/test_walltime.yaml
         -bwd ${CMAKE_SOURCE_DIR})

add_test(long_simulation_time
         ${CMAKE_SOURCE_DIR}/tools/experiments/execute_instances.py
         ${CMAKE_SOURCE_DIR}/test/test_long_simulation_time.yaml
         -bwd ${CMAKE_SOURCE_DIR})

add_test(space_sharing
         ${CMAKE_SOURCE_DIR}/tools/experiments/execute_instances.py
         ${CMAKE_SOURCE_DIR}/test/test_space_sharing.yaml
         -bwd ${CMAKE_SOURCE_DIR})

add_test(smpi
         ${CMAKE_SOURCE_DIR}/tools/experiments/execute_instances.py
         ${CMAKE_SOURCE_DIR}/test/test_smpi.yaml
         -bwd ${CMAKE_SOURCE_DIR})

add_test(batexec
         ${CMAKE_SOURCE_DIR}/tools/experiments/execute_instances.py
         ${CMAKE_SOURCE_DIR}/test/test_batexec.yaml
         -bwd ${CMAKE_SOURCE_DIR})








