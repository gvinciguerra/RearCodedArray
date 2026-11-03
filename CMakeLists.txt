cmake_minimum_required(VERSION 3.21)
project(RearCodedArray)

set(CMAKE_CXX_STANDARD 17)
enable_testing()

add_executable(example example.cpp)
add_executable(test_unsigned_char test_unsigned_char.cpp)
target_compile_options(test_unsigned_char PRIVATE -funsigned-char)
add_test(NAME unsigned_char COMMAND test_unsigned_char)
set_tests_properties(unsigned_char PROPERTIES TIMEOUT 10)
