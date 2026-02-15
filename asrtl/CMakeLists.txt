cmake_minimum_required(VERSION 3.10)

project(asrtl VERSION 0.0)

set(CMAKE_C_STANDARD 99)

add_library(asrtl util.c chann.c cobs.c)
