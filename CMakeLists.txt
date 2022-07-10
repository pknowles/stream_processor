# Copyright 2022 Pyarelal Knowles
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

cmake_minimum_required(VERSION 3.20)

project(psp)

set(CMAKE_CXX_STANDARD 17)

add_library(psp INTERFACE)
target_include_directories(psp INTERFACE include)

if(BUILD_TESTING)
add_subdirectory(test)
endif()
