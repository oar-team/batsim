cmake_minimum_required(VERSION 2.6)

add_executable(parallel_task parallel_task.c)
add_executable(batsim batsim.c)

### Add definitions for compile
target_link_libraries(parallel_task simgrid)
target_link_libraries(batsim simgrid)
