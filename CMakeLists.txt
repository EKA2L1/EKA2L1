cmake_minimum_required(VERSION 3.8)

set (GTEST_INCLUDE_DIR
	src/external/googletest/googletest/include)

add_subdirectory(src/external)
add_subdirectory(src/emu)
add_subdirectory(src/tests)
