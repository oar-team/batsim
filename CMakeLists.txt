cmake_minimum_required(VERSION 2.8)
#cmake_minimum_required(VERSION 3.0.2)

project("Batsim")

# C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
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


# Executables
add_executable(batsim batsim.cpp export.cpp ipp.cpp job_submitter.cpp jobs.cpp jobs_execution.cpp machine_range.cpp machines.cpp network.cpp profiles.cpp pstate.cpp server.cpp workload.cpp context.hpp export.hpp ipp.hpp job_submitter.hpp jobs.hpp jobs_execution.hpp machine_range.hpp machines.hpp network.hpp profiles.hpp pstate.hpp server.hpp workload.hpp)
#add_executable(batexec batexec.c job.c utils.c export.c)

# Libraries to link
target_link_libraries(batsim ${SIMGRID_LIBRARIES} ${Boost_SYSTEM_LIBRARY_DEBUG} ${Boost_FILESYSTEM_LIBRARY_DEBUG})
#target_link_libraries(batexec simgrid jansson m)


# Installation
INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/batsim
    #${CMAKE_CURRENT_BINARY_DIR}/batexec
    DESTINATION bin)


# Extra compiler flags
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set_property(TARGET batsim APPEND_STRING PROPERTY COMPILE_FLAGS "-Wall -Wextra -g")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set_property(TARGET batsim APPEND_STRING PROPERTY COMPILE_FLAGS "-Wall -Wextra -g -std=c++11")
else()
    message("Compiler is unknown, warnings will NOT be added to it")
endif()
