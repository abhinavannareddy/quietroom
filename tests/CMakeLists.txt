include(GoogleTest)

add_executable(quietroom_tests
  test_version.cpp
  test_framing.cpp
  test_fft.cpp
  test_vad.cpp
  test_noise_and_subtraction.cpp
  test_pipeline.cpp)
target_link_libraries(quietroom_tests PRIVATE quietroom GTest::gtest_main)
quietroom_target_options(quietroom_tests)
gtest_discover_tests(quietroom_tests)

# Separate binary: it replaces global operator new/delete to count allocations (F6).
add_executable(quietroom_alloc_tests test_no_alloc.cpp)
target_link_libraries(quietroom_alloc_tests PRIVATE quietroom GTest::gtest_main)
quietroom_target_options(quietroom_alloc_tests)
gtest_discover_tests(quietroom_alloc_tests)
