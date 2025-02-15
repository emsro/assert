cmake_minimum_required(VERSION 3.30)

project(assert)

include(cmake/CPM.cmake)

add_subdirectory(asrtl)
add_subdirectory(asrtc)
add_subdirectory(asrtr)

include(CTest)

enable_testing()
add_subdirectory(test)
