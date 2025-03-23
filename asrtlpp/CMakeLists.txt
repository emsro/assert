cmake_minimum_required(VERSION 3.10)

project(asrtlpp VERSION 0.0)

add_library(asrtlpp INTERFACE)
target_compile_features(asrtlpp INTERFACE cxx_std_20)
target_link_libraries(asrtlpp INTERFACE asrtl)
