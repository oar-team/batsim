cmake_minimum_required(VERSION 2.8.11)

project("Batsim")

# Options
option(enable_warnings "Enable compilation warnings" ON)
option(treat_warnings_as_errors "Treat compilation warnings as compilation errors" OFF)
option(enable_optimizations "Enable compilation-level optimizations" OFF)
option(ignore_assertions "Ignore assertions, which could make the simulation unstable, but could improve its performance" OFF)

################
# Dependencies #
################
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

## SimGrid dependency
find_package(SimGrid REQUIRED)
include_directories(${SIMGRID_INCLUDE_DIR})

## Boost dependency
find_package(Boost REQUIRED COMPONENTS system filesystem)
include_directories(${Boost_INCLUDE_DIR})

## Rapidjson dependency
find_package(rapidjson REQUIRED)
include_directories(${RAPIDJSON_INCLUDE_DIRS})

################
# Source files #
################
file(GLOB batsim_SRC
    "src/*.hpp"
    "src/*.cpp")

# Executables
add_executable(batsim ${batsim_SRC})

# Libraries to link
target_link_libraries(batsim ${SIMGRID_LIBRARIES} ${Boost_SYSTEM_LIBRARY_DEBUG}
                      ${Boost_FILESYSTEM_LIBRARY_DEBUG})

################
# Installation #
################
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/batsim
        DESTINATION bin)

##################
# Compiler flags #
##################

# Let's check that compiler versions are ok with c++11
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.8.1")
        message(FATAL_ERROR "Insufficient gcc version: 4.8.1 is needed to support C++11")
    elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5")
        message(WARNING "Old gcc version found: Using version 5 or greater is recommended")
    endif()
    set_property(TARGET batsim APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c++11")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "3.3")
        message(FATAL_ERROR "Insufficient clang version: 3.3 is needed to support C++11")
    elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "3.4")
        message(WARNING "Old clang version found: Using version 3.4 or greater is recommended")
    endif()
    set_property(TARGET batsim APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c++11")
else()
    message(WARNING "Unknown compiler. C++11 should not be enabled correctly.")
    set_property(TARGET batsim APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c++11")
endif()

# Let's enable warnings if needed
if (enable_warnings)
    set(my_flags " -Wall -Wextra")

    if (treat_warnings_as_errors)
        set(my_flags "${my_flags} -Werror")
    endif()

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set_property(TARGET batsim APPEND_STRING PROPERTY COMPILE_FLAGS ${my_flags})
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set_property(TARGET batsim APPEND_STRING PROPERTY COMPILE_FLAGS ${my_flags})
    else()
        message(WARNING "Unknown compiler. Warnings should not be enabled correctly.")
        set_property(TARGET batsim APPEND_STRING PROPERTY COMPILE_FLAGS ${my_flags})
    endif()
endif()

# Let's enable optimization if needed
if (enable_optimizations)
    set(optim_flags " -O2")

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set_property(TARGET batsim APPEND_STRING PROPERTY COMPILE_FLAGS ${optim_flags})
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set_property(TARGET batsim APPEND_STRING PROPERTY COMPILE_FLAGS ${optim_flags})
    else()
        message(WARNING "Unknown compiler. Warnings should not be enabled correctly.")
        set_property(TARGET batsim APPEND_STRING PROPERTY COMPILE_FLAGS ${optim_flags})
    endif()
endif()

if (ignore_assertions)
    target_compile_definitions(batsim PRIVATE NDEBUG)
endif()
