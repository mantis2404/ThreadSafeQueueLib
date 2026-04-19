cmake_minimum_required(VERSION 3.14)

# Name your project
project(TSFQueue_Tests VERSION 1.0 LANGUAGES CXX)

# Force modern C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tell CMake where to find your queue header files (assuming they are in the same folder or an 'include' folder)
# If tsfqueue.hpp is in the root directory, leave this as ${CMAKE_CURRENT_SOURCE_DIR}.
# If it is inside a folder named 'include', change this to ${CMAKE_CURRENT_SOURCE_DIR}/include
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Automatically download and build Google Test from the official repository
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Create your executable from the test file
add_executable(test_spsc tests/test_spsc.cpp)

# Enable the FAST macro from your umbrella header (Uncomment if you want to test the fast version)
# target_compile_definitions(test_spsc PRIVATE FAST)

# Link Google Test and the OS Threading library (pthread) to your executable
find_package(Threads REQUIRED)
target_link_libraries(test_spsc PRIVATE gtest_main Threads::Threads)

# Register the test with CMake so you can run it using the 'ctest' command
include(GoogleTest)
gtest_discover_tests(test_spsc)