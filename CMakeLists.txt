cmake_minimum_required(VERSION 3.0.2)

project("Batsim")

set(CMAKE_BUILD_TYPE Debug)

# Enable C11 or at least C99
include(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG("-std=c11" COMPILER_SUPPORTS_C11)
CHECK_C_COMPILER_FLAG("-std=c99" COMPILER_SUPPORTS_C99)
if(COMPILER_SUPPORTS_C11)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")
elseif(COMPILER_SUPPORTS_C99)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
else()
        message(STATUS "The compiler ${CMAKE_C_COMPILER} has no C11 nor C99 support. Please use a different C compiler.")
endif()

add_executable(batsim batsim.c job.c utils.c export.c)
add_executable(batexec batexec.c job.c utils.c export.c)

### Add definitions for compile
target_link_libraries(batsim simgrid jansson m)
target_link_libraries(batexec simgrid jansson m)
