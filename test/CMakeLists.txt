# Copyright 2022 Pyarelal Knowles
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

cmake_minimum_required(VERSION 3.20)

enable_testing()

add_executable(
  tests
  src/main.cpp
)

target_link_libraries(
  tests
  psp
  gtest_main
)

add_subdirectory(googletest)
include(GoogleTest)
gtest_discover_tests(tests)
