include(GoogleTest)

add_executable(quietroom_tests
  test_version.cpp
  test_framing.cpp
  test_fft.cpp
  test_vad.cpp
  test_noise_and_subtraction.cpp
  test_pipeline.cpp
  test_wav.cpp)
target_link_libraries(quietroom_tests PRIVATE quietroom GTest::gtest_main)
quietroom_target_options(quietroom_tests)
gtest_discover_tests(quietroom_tests)

# Separate binary: it replaces global operator new/delete to count allocations (F6).
add_executable(quietroom_alloc_tests test_no_alloc.cpp)
target_link_libraries(quietroom_alloc_tests PRIVATE quietroom GTest::gtest_main)
quietroom_target_options(quietroom_alloc_tests)
gtest_discover_tests(quietroom_alloc_tests)

# Runs the real CLI binary on a committed noisy clip (F7 regression test).
add_executable(quietroom_cli_tests test_cli_regression.cpp)
target_link_libraries(quietroom_cli_tests PRIVATE quietroom GTest::gtest_main)
target_compile_definitions(quietroom_cli_tests PRIVATE
  QUIETROOM_CLI_PATH="$<TARGET_FILE:quietroom_cli>"
  QUIETROOM_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
add_dependencies(quietroom_cli_tests quietroom_cli)
quietroom_target_options(quietroom_cli_tests)
gtest_discover_tests(quietroom_cli_tests)
