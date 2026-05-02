cmake_minimum_required(VERSION 3.20)
project(asrt_reactor_consumer)

# Locate the installed asrt package
find_package(asrt REQUIRED)

add_executable(reactor_smoke main.c)
target_link_libraries(reactor_smoke PRIVATE asrt::asrtl asrt::asrtr)
