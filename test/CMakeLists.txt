cpmaddpackage("gh:doctest/doctest@2.4.12")

add_executable(assertc_test asrtc_test.cpp)
target_link_libraries(assertc_test PRIVATE doctest::doctest asrtc)
target_compile_features(assertc_test PUBLIC cxx_std_20)
add_test(NAME assrtc_test COMMAND assertc_test)

add_executable(assertr_test asrtr_test.cpp)
target_link_libraries(assertr_test PRIVATE doctest::doctest asrtr)
target_compile_features(assertr_test PUBLIC cxx_std_20)
add_test(NAME assrtr_test COMMAND assertr_test)

add_executable(assertl_test asrtl_test.cpp)
target_link_libraries(assertl_test PRIVATE doctest::doctest asrtl)
target_compile_features(assertl_test PUBLIC cxx_std_20)
add_test(NAME assrtl_test COMMAND assertl_test)

add_executable(mutatest mutatest.cpp)
target_link_libraries(mutatest PUBLIC asrtrpp asrtcpp CLI11::CLI11)
target_compile_features(mutatest PUBLIC cxx_std_20)
add_test(NAME mutatest COMMAND mutatest)

# Interactive progress-bar demo — not a CTest test, just a visual sanity check
# Run manually:  ./_build/test/pbar_demo [--fast] [--total 40]
add_executable(pbar_demo pbar_demo.cpp)
target_compile_features(pbar_demo PUBLIC cxx_std_20)
target_link_libraries(pbar_demo PRIVATE CLI11::CLI11)

if(ASSERT_ENABLE_COVERAGE)
  add_test(
    NAME coverage
    COMMAND gcovr --cobertura-pretty --cobertura cobertura.xml -r
            ${CMAKE_SOURCE_DIR}/
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
