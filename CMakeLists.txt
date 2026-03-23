cmake_minimum_required(VERSION 3.30)

project(assert)

include(cmake/CPM.cmake)

cpmaddpackage("gh:koniarik/ecor#f2e7bc18523078268924f54fed5c440c8a70db04")

add_subdirectory(asrtl)
add_subdirectory(asrtc)
add_subdirectory(asrtr)

add_subdirectory(asrtlpp)
add_subdirectory(asrtcpp)
add_subdirectory(asrtrpp)
add_subdirectory(asrtio)

include(CTest)

enable_testing()
add_subdirectory(test)
