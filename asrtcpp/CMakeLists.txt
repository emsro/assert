cmake_minimum_required(VERSION 3.10)

project(asrtcpp VERSION 0.0)

add_library(asrtcpp controller.cpp)
target_compile_features(asrtcpp PUBLIC cxx_std_20)
target_link_libraries(asrtcpp PUBLIC asrtc)
