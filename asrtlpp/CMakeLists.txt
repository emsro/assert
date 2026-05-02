cmake_minimum_required(VERSION 3.10)

project(asrtlpp VERSION 0.0)

add_library(asrtlpp INTERFACE)
add_library(asrt::asrtlpp ALIAS asrtlpp)
target_compile_features(asrtlpp INTERFACE cxx_std_20)
target_link_libraries(asrtlpp INTERFACE asrtl ecor)

install(TARGETS asrtlpp EXPORT asrtCxxTargets)
install(
  DIRECTORY ./
  DESTINATION include/asrtlpp
  FILES_MATCHING
  PATTERN "*.hpp")
