cmake_minimum_required(VERSION 3.10)

project(asrtrpp VERSION 0.0)

add_library(asrtrpp reactor.cpp)
target_compile_features(asrtrpp PUBLIC cxx_std_20)
target_link_libraries(asrtrpp PUBLIC asrtr)
