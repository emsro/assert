cmake_minimum_required(VERSION 3.10)

project(asrtrpp VERSION 0.0)

add_library(asrtrpp reactor.cpp)
add_library(asrt::asrtrpp ALIAS asrtrpp)
target_compile_features(asrtrpp PUBLIC cxx_std_20)
target_link_libraries(asrtrpp PUBLIC asrtr asrtlpp)

install(
  TARGETS asrtrpp
  EXPORT asrtCxxTargets
  ARCHIVE DESTINATION lib)
install(
  DIRECTORY ./
  DESTINATION include/asrtrpp
  FILES_MATCHING
  PATTERN "*.hpp")
