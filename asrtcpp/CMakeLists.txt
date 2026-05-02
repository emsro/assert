cmake_minimum_required(VERSION 3.10)

project(asrtcpp VERSION 0.0)

add_library(asrtcpp stream.cpp)
add_library(asrt::asrtcpp ALIAS asrtcpp)
target_compile_features(asrtcpp PUBLIC cxx_std_20)
target_link_libraries(asrtcpp PUBLIC asrtc asrtlpp)

install(
  TARGETS asrtcpp
  EXPORT asrtCxxTargets
  ARCHIVE DESTINATION lib)
install(
  DIRECTORY ./
  DESTINATION include/asrtcpp
  FILES_MATCHING
  PATTERN "*.hpp")
