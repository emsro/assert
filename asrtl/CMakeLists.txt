cmake_minimum_required(VERSION 3.10)

project(asrtl VERSION 0.1.0)

set(CMAKE_C_STANDARD 11)

configure_file(proto_version.h.in proto_version.h)

add_library(asrtl util.c chann.c cobs.c flat_tree.c allocator.c)
add_library(asrt::asrtl ALIAS asrtl)
target_include_directories(
  asrtl
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         $<INSTALL_INTERFACE:include>)

install(
  TARGETS asrtl
  EXPORT asrtCTargets
  ARCHIVE DESTINATION lib)
install(
  DIRECTORY ./
  DESTINATION include/asrtl
  FILES_MATCHING
  PATTERN "*.h")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/proto_version.h
        DESTINATION include/asrtl)
