cpmaddpackage("gh:doctest/doctest@2.4.12")

add_executable(asrtc_test asrtc_test.cpp)
target_link_libraries(asrtc_test PRIVATE doctest::doctest asrtc)
target_compile_features(asrtc_test PUBLIC cxx_std_20)
add_test(NAME asrtc_test COMMAND asrtc_test)

add_executable(asrtr_test asrtr_test.cpp)
target_link_libraries(asrtr_test PRIVATE doctest::doctest asrtr)
target_compile_features(asrtr_test PUBLIC cxx_std_20)
add_test(NAME asrtr_test COMMAND asrtr_test)

add_executable(asrtl_test asrtl_test.cpp)
target_link_libraries(asrtl_test PRIVATE doctest::doctest asrtl)
target_compile_features(asrtl_test PUBLIC cxx_std_20)
add_test(NAME asrtl_test COMMAND asrtl_test)

add_executable(asrtcpp_test asrtcpp_test.cpp)
target_link_libraries(asrtcpp_test PRIVATE doctest::doctest asrtcpp asrtrpp)
target_compile_features(asrtcpp_test PUBLIC cxx_std_20)
add_test(NAME asrtcpp_test COMMAND asrtcpp_test)

add_executable(asrtrpp_test asrtrpp_test.cpp)
target_link_libraries(asrtrpp_test PRIVATE doctest::doctest asrtrpp)
target_compile_features(asrtrpp_test PUBLIC cxx_std_20)
add_test(NAME asrtrpp_test COMMAND asrtrpp_test)

add_executable(asrtio_test asrtio_test.cpp)
target_link_libraries(asrtio_test PRIVATE doctest::doctest asrtrpp asrtcpp uv)
target_compile_features(asrtio_test PUBLIC cxx_std_20)
add_test(NAME asrtio_test COMMAND asrtio_test)

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
