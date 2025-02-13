cpmaddpackage("gh:ThrowTheSwitch/Unity@2.6.1")

add_executable(assert_test asrtr_test.c)
target_link_libraries(assert_test PUBLIC unity asrtr)
add_test(NAME assrt_test COMMAND assert_test)
