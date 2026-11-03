cmake_minimum_required(VERSION 3.21)
project(RearCodedArray)

set(CMAKE_CXX_STANDARD 17)
enable_testing()

add_executable(test_headers_padding test_headers_padding.cpp)
add_test(NAME headers_padding COMMAND test_headers_padding)
add_executable(example example.cpp)
