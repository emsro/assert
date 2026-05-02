cmake_minimum_required(VERSION 3.20)
project(asrt_controller_consumer CXX)

# Locate the installed asrt package
find_package(asrt REQUIRED)

add_executable(controller_smoke main.cpp)
target_link_libraries(controller_smoke PRIVATE asrt::asrtl asrt::asrtc)
target_compile_features(controller_smoke PRIVATE cxx_std_20)
