cmake_minimum_required(VERSION 3.21)
project(RearCodedArray)

set(CMAKE_CXX_STANDARD 17)
enable_testing()

add_executable(test_buffer test_buffer.cpp)
add_test(NAME buffer COMMAND test_buffer)
add_executable(example example.cpp)
