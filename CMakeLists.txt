cmake_minimum_required(VERSION 3.21)
project(RearCodedArray)

set(CMAKE_CXX_STANDARD 17)
enable_testing()

add_executable(example example.cpp)
add_executable(test_rank test_rank.cpp)
add_test(NAME rank COMMAND test_rank)
