# Copyright 2022 Pyarelal Knowles
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

cmake_minimum_required(VERSION 3.20)

# Unit tests
add_executable(unit_tests src/unit.cpp)
target_link_libraries(unit_tests psp gtest_main)
add_subdirectory(googletest)
include(GoogleTest)
gtest_discover_tests(unit_tests)

# Fuzz testing
add_executable(fuzz_tests src/fuzz.cpp)
target_link_libraries(fuzz_tests psp $<$<C_COMPILER_ID:Clang>:-fsanitize=fuzzer,address>)
target_compile_options(fuzz_tests PRIVATE $<$<C_COMPILER_ID:Clang>:-g -O1 -fsanitize=fuzzer,address>)
add_test(NAME fuzz_tests COMMAND fuzz_tests)
