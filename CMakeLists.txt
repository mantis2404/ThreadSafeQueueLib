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
FetchContent_MakeAvailable(googletest)

add_executable(test_spsc tests/test_spsc.cpp)
add_executable(test_mpmc tests/test_mpmc.cpp)
add_executable(test_mpsc tests/test_mpsc.cpp)
add_executable(test_mpmc_bounded tests/test_mpmc_bounded.cpp)

find_package(Threads REQUIRED)
target_link_libraries(test_spsc PRIVATE gtest_main Threads::Threads)
target_link_libraries(test_mpmc PRIVATE gtest_main Threads::Threads)
target_link_libraries(test_mpsc PRIVATE gtest_main Threads::Threads)
target_link_libraries(test_mpmc_bounded PRIVATE gtest_main Threads::Threads)

include(GoogleTest)
gtest_discover_tests(test_spsc)
gtest_discover_tests(test_mpmc)
gtest_discover_tests(test_mpsc)
gtest_discover_tests(test_mpmc_bounded)