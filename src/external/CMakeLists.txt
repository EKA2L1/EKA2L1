add_library(miniz miniz/miniz.h miniz/miniz.c)
target_include_directories(miniz PUBLIC miniz)

# Force Boost directory for Dynarmic - we never use one
set (BOOST_INCLUDEDIR "${CMAKE_CURRENT_SOURCE_DIR}/ext-boost/" CACHE PATH "Boost include directory")

# PugiXML
add_library(pugixml
    pugixml/src/pugiconfig.hpp
    pugixml/src/pugixml.cpp
    pugixml/src/pugixml.hpp)

target_include_directories(pugixml PUBLIC pugixml/src)

# Nativefilediag
add_subdirectory(filediag)

# fmt
add_subdirectory(fmt)
add_library(fmt::fmt ALIAS fmt)
set (fmt_DIR fmt)

# Pybind11
if (ENABLE_SCRIPTING_ABILITY)
	set (PYBIND11_TEST OFF CACHE BOOL "" FORCE)
	add_subdirectory(pybind11)
endif()

# SPDLOG - Build examples off
set (SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) 
set (SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
# set (SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)

add_subdirectory(spdlog)

# GLM
set (GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
add_subdirectory(glm)

# Dynarmic: disable tests
set(DYNARMIC_TESTS OFF CACHE BOOL "")
set(DYNARMIC_NO_BUNDLED_FMT ON CACHE BOOL "")
add_subdirectory(dynarmic)

# GLFW - Disable docs, tests and examples
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(glfw)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DPy_DEBUG)
    message("Add debug to prevent crash for PyBind11")
endif()

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Enable testing" FORCE)
add_subdirectory(yaml-cpp)

add_library(imgui imgui/imgui.h imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_internal.h
    imgui/imconfig.h
    imgui/imstb_rectpack.h
    imgui/imstb_textedit.h
    imgui/imstb_truetype.h
    imgui/misc/cpp/imgui_stdlib.cpp
    imgui/misc/cpp/imgui_stdlib.h
)

if (MSVC)
    add_library(libdemangle
		libdemangle/include/cxxabi.h
		libcxxabi/src/cxa_demangle.cpp)

	target_include_directories(libdemangle PUBLIC libdemangle/include)
endif()

target_include_directories(imgui PUBLIC imgui)

add_library(glad glad/include/glad/glad.h glad/src/glad.c)
target_include_directories(glad PUBLIC glad/include)

#stb
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE stb)

#catch2
set (CATCH_BUILD_TESTING OFF CACHE BOOL "Turn off building testing" FORCE)
set (CATCH_ENABLE_WERROR OFF CACHE BOOL "Disable werror" FORCE)

add_subdirectory(Catch2)

add_library(unicorn INTERFACE)

# Microprofile
add_library(microprofile STATIC microprofile/microprofile.cpp microprofile/microprofile.h)
target_include_directories(microprofile PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/microprofile")
set_property(TARGET microprofile PROPERTY CXX_STANDARD 11)
target_compile_definitions(microprofile PUBLIC MICROPROFILE_ENABLED=0 MICROPROFILE_GPU_TIMERS=0)

if(APPLE)
	target_include_directories(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/macos/include")
	target_link_libraries(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/macos/lib/libunicorn.a")
elseif(WIN32)
	target_include_directories(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/windows/include")
	target_link_libraries(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/windows/unicorn.lib")
elseif(UNIX)
    target_include_directories(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/unix/include")
    target_link_libraries(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/unix/lib/libunicorn.a")
endif()

## XXHash
add_library(xxHash INTERFACE)
target_include_directories(xxHash INTERFACE xxHash)

if (BUILD_TOOLS)
    find_package(LibClang QUIET)

    if (LIBCLANG_FOUND)
        set (LIBCLANG_AVAILABLE TRUE PARENT_SCOPE)

        add_library(libclang INTERFACE)
        target_link_libraries(libclang INTERFACE ${libclang_lib})
        target_include_directories(libclang INTERFACE ${libclang_include_dir})
    endif()
endif()

# Find Vulkan SDK
find_package(Vulkan QUIET)

if (NOT Vulkan_FOUND)
    message("Couldn't find Vulkan SDK, building Vulkan backend not enabled!")
    set (BUILD_WITH_VULKAN 0 PARENT_SCOPE)
endif()

add_library(Vulkan INTERFACE)
target_include_directories(Vulkan INTERFACE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(Vulkan INTERFACE ${Vulkan_LIBRARIES})

option(CAPSTONE_BUILD_SHARED "Build shared library" OFF)
option(CAPSTONE_BUILD_TESTS "Build tests" OFF)
option(CAPSTONE_ARM_SUPPORT "ARM support" ON)
option(CAPSTONE_ARM64_SUPPORT "ARM64 support" OFF)
option(CAPSTONE_MIPS_SUPPORT "MIPS support" OFF)
option(CAPSTONE_PPC_SUPPORT "PowerPC support" OFF)
option(CAPSTONE_SPARC_SUPPORT "Sparc support" OFF)
option(CAPSTONE_SYSZ_SUPPORT "SystemZ support" OFF)
option(CAPSTONE_XCORE_SUPPORT "XCore support" OFF)
option(CAPSTONE_X86_SUPPORT "x86 support" OFF)

add_subdirectory(capstone EXCLUDE_FROM_ALL)

set (capstone_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/capstone/include" PARENT_SCOPE)
set (YAML_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/yaml-cpp/include" PARENT_SCOPE)
set (LIBCXXABI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libcxxabi/include" PARENT_SCOPE)
set (FRSML_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/FRSML/include" PARENT_SCOPE)
set (GLM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/glm" PARENT_SCOPE)
set (EPOC32_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/epocapi/epoc32/include" PARENT_SCOPE)
