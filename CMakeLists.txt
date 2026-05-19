cmake_minimum_required(VERSION 3.14)
project(ThreadsafeQueueLib VERSION 1.0 LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.12.1
)

# Google Benchmark  (BENCHMARK_ENABLE_TESTING=OFF avoids pulling GTest twice)
FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG        v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest googlebenchmark)

find_package(Threads REQUIRED)

add_executable(test_spsc tests/test_spsc.cpp)
add_executable(test_mpmc tests/test_mpmc.cpp)
add_executable(test_mpsc tests/test_mpsc.cpp)
add_executable(test_mpmc_bounded tests/test_mpmc_bounded.cpp)

target_link_libraries(test_spsc PRIVATE gtest_main Threads::Threads)
target_link_libraries(test_mpmc PRIVATE gtest_main Threads::Threads)
target_link_libraries(test_mpsc PRIVATE gtest_main Threads::Threads)
target_link_libraries(test_mpmc_bounded PRIVATE gtest_main Threads::Threads)

include(GoogleTest)
gtest_discover_tests(test_spsc)
gtest_discover_tests(test_mpmc)
gtest_discover_tests(test_mpsc)
gtest_discover_tests(test_mpmc_bounded)

add_executable(bench_spsc  benchmarking/bench_spsc.cpp)
add_executable(bench_mpmc  benchmarking/bench_mpmc.cpp)
add_executable(bench_mpsc  benchmarking/bench_mpsc.cpp)

foreach(_bench bench_spsc bench_mpmc bench_mpsc)
  target_link_libraries(${_bench} PRIVATE benchmark::benchmark Threads::Threads)
  target_compile_options(${_bench} PRIVATE -O3 -DNDEBUG)
endforeach()

add_custom_target(benchmarks DEPENDS bench_spsc bench_mpmc bench_mpsc)