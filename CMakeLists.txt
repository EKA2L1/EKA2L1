cmake_minimum_required(VERSION 3.8)

project (EKA2L1)
set (CMAKE_CXX_STANDARD 17)

option(EKA2L1_BUILD_TOOLS "Build EKA2L1's tool" ON)
option(EKA2L1_BUILD_TESTS "Build EKA2L1's tests" ON)
option(EKA2L1_ENABLE_SCRIPTING_ABILITY "Enable to script with Python or Lua" ON)
option(EKA2L1_ENABLE_UNEXPECTED_EXCEPTION_HANDLER "Enable EKA2L1 to dump unexpected exception" OFF)
option(EKA2L1_BUILD_VULKAN_BACKEND "Build Vulkan backend" ON)

set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set (ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")

    message("Using CCache for building")
endif()

if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
endif(MSVC)

if (EKA2L1_BUILD_VULKAN_BACKEND)
    set (BUILD_WITH_VULKAN 1)
else()
    set (BUILD_WITH_VULKAN 0)
endif()

if (EKA2L1_ENABLE_SCRIPTING_ABILITY)
    set (ENABLE_SCRIPTING 1)

    # Annoying for other devs working for both android and pc, so disable now. We can live with lua
    #if (ANDROID OR CI)
        set(ENABLE_PYTHON_SCRIPTING 0)
    #else()
    #    message("Enable scripting with Python with build")
    #    set(ENABLE_PYTHON_SCRIPTING 1)
    #endif()

    if (UNIX AND (NOT APPLE))
        # Pybind11 requires this on
        if (ENABLE_PYTHON_SCRIPTING)
            set(CMAKE_POSITION_INDEPENDENT_CODE ON)
        endif()

        set(CMAKE_CXX_STANDARD_LIBRARIES -ldl)
    endif()
else (EKA2L1_ENABLE_SCRIPTING_ABILITY)
    set (ENABLE_SCRIPTING 0)
endif(EKA2L1_ENABLE_SCRIPTING_ABILITY)

set (ENABLE_SEH_HANDLER 0)

if (EKA2L1_ENABLE_UNEXPECTED_EXCEPTION_HANDLER)
    set(ENABLE_SEH_HANDLER 1)
endif (EKA2L1_ENABLE_UNEXPECTED_EXCEPTION_HANDLER)

add_subdirectory(src/patch)
add_subdirectory(src/external)
add_subdirectory(src/emu)

if (EKA2L1_BUILD_TOOLS)
    add_subdirectory(src/tools)
endif()

if (EKA2L1_BUILD_TESTS)
    enable_testing()
    add_subdirectory(src/tests)
endif()
