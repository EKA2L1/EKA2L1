add_library(miniz miniz/miniz.h miniz/miniz.c)
target_include_directories(miniz PUBLIC miniz)

# Force Boost directory for Dynarmic - we never use one
set (BOOST_INCLUDEDIR "${CMAKE_CURRENT_SOURCE_DIR}/ext-boost/" CACHE PATH "Boost include directory")
if (ANDROID)
    add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY
            -DBOOST_SYSTEM_NO_LIB
            -DBOOST_DATE_TIME_NO_LIB
            -DBOOST_REGEX_NO_LIB
            -DBOOST_ALL_NO_LIB
            )
    set (BOOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ext-boost/")
    add_library(boost INTERFACE)
    target_include_directories(boost SYSTEM INTERFACE ${BOOST_INCLUDEDIR})
endif()

# PugiXML
add_library(pugixml
    pugixml/src/pugiconfig.hpp
    pugixml/src/pugixml.cpp
    pugixml/src/pugixml.hpp)

target_include_directories(pugixml PUBLIC pugixml/src)

# Nativefilediag
if (NOT ANDROID)
    add_subdirectory(filediag)
endif()

# fmt
add_subdirectory(fmt)
add_library(fmt::fmt ALIAS fmt)
set (fmt_DIR fmt)

# Pybind11
if (EKA2L1_ENABLE_SCRIPTING_ABILITY)
	option (PYBIND11_TEST "Enable pybind11 tests" OFF)
	add_subdirectory(pybind11)
endif()

# SPDLOG - Build examples off
option (SPDLOG_BUILD_EXAMPLES "Build spdlog Examples" OFF) 
option (SPDLOG_BUILD_BENCH "Build spdlog benchmarks" OFF)
# set (SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)

add_subdirectory(spdlog)

# GLM
option (GLM_TEST_ENABLE "Enable glm tests" OFF)
add_subdirectory(glm)

# Dynarmic: disable tests
option(DYNARMIC_TESTS "Enable dynarmic tests" ON)
option(DYNARMIC_NO_BUNDLED_FMT "No bundled fmt for Dynarmic" ON)
add_subdirectory(dynarmic)

# GLFW - Disable docs, tests and examples
option(GLFW_BUILD_DOCS "Build glfw documents" OFF)
option(GLFW_BUILD_TESTS "Build glfw tests" OFF)
option(GLFW_BUILD_EXAMPLES "Build glfw examples" OFF)

if (NOT ANDROID)
    add_subdirectory(glfw)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DPy_DEBUG)
    message("Add debug to prevent crash for PyBind11")
endif()

option(YAML_CPP_BUILD_TESTS "Enable YAML cpp testing" OFF)
add_subdirectory(yaml-cpp)

if (NOT ANDROID)
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
    target_include_directories(imgui PUBLIC imgui)
endif()

if (MSVC)
    add_library(libdemangle
		libdemangle/include/cxxabi.h
		libcxxabi/src/cxa_demangle.cpp)

    target_include_directories(libdemangle PUBLIC libdemangle/include)
endif()

add_library(glad glad/include/glad/glad.h glad/src/glad.c)
target_include_directories(glad PUBLIC glad/include)

#stb
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE stb)

#catch2
option (CATCH_BUILD_TESTING "Build catch testing" OFF)
option (CATCH_ENABLE_WERROR "Disable catch werror" OFF)

add_subdirectory(Catch2)

# Microprofile
add_library(microprofile STATIC microprofile/microprofile.cpp microprofile/microprofile.h)
target_include_directories(microprofile PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/microprofile")
set_property(TARGET microprofile PROPERTY CXX_STANDARD 11)
target_compile_definitions(microprofile PUBLIC MICROPROFILE_ENABLED=0 MICROPROFILE_GPU_TIMERS=0)

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

# Cubeb
option (BUILD_TOOLS "Build tools" OFF)
option (BUILD_TESTS "Build cubeb tests" OFF)
add_subdirectory(cubeb)

# FFMPEG
add_subdirectory(ffmpeg)

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
