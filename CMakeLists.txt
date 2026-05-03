cmake_minimum_required(VERSION 3.30)

project(assert)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(cmake/CPM.cmake)

cpmaddpackage("gh:koniarik/ecor@0.1.8")

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

# --- install / package config ---

# C libraries (asrtl, asrtc, asrtr) — no external dependencies
install(
  EXPORT asrtCTargets
  NAMESPACE asrt::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/asrt)

configure_package_config_file(
  cmake/asrtConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/asrtConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/asrt)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/asrtConfig.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/asrt)

# C++ wrappers (asrtlpp, asrtcpp, asrtrpp) — require ecor
install(
  EXPORT asrtCxxTargets
  NAMESPACE asrt::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/asrtCxx)

configure_package_config_file(
  cmake/asrtCxxConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/asrtCxxConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/asrtCxx)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/asrtCxxConfig.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/asrtCxx)
