cmake_minimum_required(VERSION 2.6)

project("Batsim")

set(CMAKE_BUILD_TYPE Debug)

add_executable(batsim batsim.c)

### Add definitions for compile
target_link_libraries(batsim simgrid jansson)
