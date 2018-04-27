cmake_minimum_required(VERSION 3.8)

set (GTEST_INCLUDE_DIR
	src/external/googletest/googletest/include)

set (YAML_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/external/yaml-cpp/include")

set (CMAKE_CXX_STANDARD 17)

add_subdirectory(src/external)
add_subdirectory(src/tests)
add_subdirectory(src/emu)

add_subdirectory(tools)
