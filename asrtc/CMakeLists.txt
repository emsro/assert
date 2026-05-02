cmake_minimum_required(VERSION 3.10)

project(asrtc VERSION 0.0)

set(CMAKE_C_STANDARD 11)

add_library(asrtc cntr_assm.c collect.c controller.c diag.c param.c stream.c)
add_library(asrt::asrtc ALIAS asrtc)
target_link_libraries(asrtc PUBLIC asrtl)

install(
  TARGETS asrtc
  EXPORT asrtCTargets
  ARCHIVE DESTINATION lib)
install(
  DIRECTORY ./
  DESTINATION include/asrtc
  FILES_MATCHING
  PATTERN "*.h")
