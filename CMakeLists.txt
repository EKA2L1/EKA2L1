cmake_minimum_required(VERSION 3.8)

project (EKA2L1)
set (CMAKE_CXX_STANDARD 17)

option(BUILD_TOOLS "Build EKA2L1's tool" ON)
option(BUILD_TESTS "Build EKA2L1's tests" ON)

set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set (ROOT ${CMAKE_CURRENT_SOURCE_DIR})

if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
else ()
endif(MSVC)

add_subdirectory(src/external)
add_subdirectory(src/emu)

if (BUILD_TOOLS)
    add_subdirectory(src/tools)
endif()

if (BUILD_TESTS)
    add_subdirectory(src/tests)
endif()