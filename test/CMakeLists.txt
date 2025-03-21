cpmaddpackage("gh:ThrowTheSwitch/Unity@2.6.1")

add_executable(assertc_test asrtc_test.c)
target_link_libraries(assertc_test PUBLIC unity asrtc)
add_test(NAME assrtc_test COMMAND assertc_test)

add_executable(assertr_test asrtr_test.c)
target_link_libraries(assertr_test PUBLIC unity asrtr)
add_test(NAME assrtr_test COMMAND assertr_test)

add_executable(assertl_test asrtl_test.c)
target_link_libraries(assertl_test PUBLIC unity asrtl)
add_test(NAME assrtl_test COMMAND assertl_test)

add_executable(rsim rsim.cpp)
target_link_libraries(rsim PUBLIC asrtrpp CLI11::CLI11 Boost::asio)

if(ASSERT_ENABLE_COVERAGE)
  add_test(
    NAME coverage
    COMMAND gcovr --cobertura-pretty --cobertura cobertura.xml -r
            ${CMAKE_SOURCE_DIR}/
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
