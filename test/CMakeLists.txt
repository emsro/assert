cpmaddpackage("gh:ThrowTheSwitch/Unity@2.6.1")

add_executable(assertr_test asrtr_test.c)
target_link_libraries(assertr_test PUBLIC unity asrtr)
add_test(NAME assrtr_test COMMAND assertr_test)

add_executable(assertl_test asrtl_test.c)
target_link_libraries(assertl_test PUBLIC unity asrtl)
add_test(NAME assrtl_test COMMAND assertl_test)

if(ASSERT_ENABLE_COVERAGE)
  add_test(
    NAME coverage
    COMMAND gcovr --cobertura-pretty --cobertura cobertura.xml -r
            ${CMAKE_SOURCE_DIR}/
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
