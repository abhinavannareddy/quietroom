add_executable(quietroom_bench bench_main.cpp)
target_include_directories(quietroom_bench PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(quietroom_bench PRIVATE quietroom benchmark::benchmark)
quietroom_target_options(quietroom_bench)
