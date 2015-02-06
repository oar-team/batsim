cmake_minimum_required(VERSION 2.6)

add_executable(batsim batsim.c)

### Add definitions for compile
target_link_libraries(batsim simgrid)
