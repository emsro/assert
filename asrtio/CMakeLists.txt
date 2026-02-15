cmake_minimum_required(VERSION 3.10)

project(asrtio VERSION 0.0)

cpmaddpackage("gh:CLIUtils/CLI11@2.5.0")
cpmaddpackage("gh:libuv/libuv@1.51.0")

add_executable(asrtio main.cpp)
target_compile_features(asrtio PUBLIC cxx_std_20)
target_link_libraries(asrtio PUBLIC asrtcpp asrtrpp CLI11::CLI11 uv)
