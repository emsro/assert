cmake_minimum_required(VERSION 3.10)

project(asrtr VERSION 0.0)

set(CMAKE_C_STANDARD 99)

add_library(asrtr asrtr.c)
target_link_libraries(asrtr PUBLIC asrtl)
