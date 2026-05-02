cmake_minimum_required(VERSION 3.10)

project(asrtr VERSION 0.0)

set(CMAKE_C_STANDARD 11)

add_library(asrtr reactor.c diag.c collect.c param.c stream.c reac_assm.c)
target_link_libraries(asrtr PUBLIC asrtl)
