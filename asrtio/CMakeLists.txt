cmake_minimum_required(VERSION 3.10)

project(asrtio VERSION 0.0)

cpmaddpackage("gh:CLIUtils/CLI11@2.5.0")
cpmaddpackage(
  NAME
  Boost
  VERSION
  URL
  https://github.com/boostorg/boost/releases/download/boost-1.86.0/boost-1.86.0-cmake.tar.xz
  URL_HASH
  SHA256=2c5ec5edcdff47ff55e27ed9560b0a0b94b07bd07ed9928b476150e16b0efc57
  OPTIONS
  "BOOST_ENABLE_CMAKE ON"
  "BOOST_SKIP_INSTALL_RULES ON"
  "BUILD_SHARED_LIBS OFF"
  "BOOST_INCLUDE_LIBRARIES container\\\;asio\\\;process" # Note the escapes!
)

add_executable(asrtio main.cpp)
target_compile_features(asrtio PUBLIC cxx_std_20)
target_link_libraries(asrtio PUBLIC asrtcpp CLI11::CLI11 Boost::asio
                                    Boost::process)
