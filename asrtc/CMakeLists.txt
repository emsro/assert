cmake_minimum_required(VERSION 3.10)

project(asrtc VERSION 0.0)

set(CMAKE_C_STANDARD 11)

add_library(asrtc assembly.c collect.c controller.c diag.c param.c stream.c)
target_link_libraries(asrtc PUBLIC asrtl)
