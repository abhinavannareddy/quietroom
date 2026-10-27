include(GoogleTest)

add_executable(quietroom_tests test_version.cpp test_framing.cpp test_fft.cpp)
target_link_libraries(quietroom_tests PRIVATE quietroom GTest::gtest_main)
quietroom_target_options(quietroom_tests)
gtest_discover_tests(quietroom_tests)
