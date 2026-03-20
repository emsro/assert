cmake_minimum_required(VERSION 3.10)

project(asrtl VERSION 0.1.0)

set(CMAKE_C_STANDARD 11)

configure_file(proto_version.h.in proto_version.h)

add_library(asrtl util.c chann.c cobs.c flat_tree.c allocator.c)
target_include_directories(asrtl PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
