include(ExternalProject)
include(CheckSymbolExists)

add_library(miniz miniz/miniz.h miniz/miniz.c)
target_include_directories(miniz PUBLIC miniz)

# Force Boost directory for Dynarmic - we never use one
set (Boost_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ext-boost/" CACHE PATH "Boost include directory")
if (ANDROID)
    add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY
            -DBOOST_SYSTEM_NO_LIB
            -DBOOST_DATE_TIME_NO_LIB
            -DBOOST_REGEX_NO_LIB
            -DBOOST_ALL_NO_LIB
            )
    set (BOOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ext-boost/")
    add_library(boost INTERFACE)
    target_include_directories(boost SYSTEM INTERFACE ${Boost_INCLUDE_DIR})
endif()

# PugiXML
add_library(pugixml
    pugixml/src/pugiconfig.hpp
    pugixml/src/pugixml.cpp
    pugixml/src/pugixml.hpp)

target_include_directories(pugixml PUBLIC pugixml/src)

# fmt
add_subdirectory(fmt)
add_library(fmt::fmt ALIAS fmt)
set (fmt_DIR fmt)

set(CMAKE_REQUIRED_QUIET 1)

if (MSVC)
    check_symbol_exists("_M_ARM" "" ARCHITECTURE_ARM32)
    check_symbol_exists("_M_ARM64" "" ARCHITECTURE_AARCH64)
else()
    check_symbol_exists("__arm__" "" ARCHITECTURE_ARM32)
    check_symbol_exists("__aarch64__" "" ARCHITECTURE_AARCH64)
endif()

unset(CMAKE_REQUIRED_QUIET)

if (EKA2L1_ENABLE_SCRIPTING_ABILITY)
    set(LUA_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/luaorg)

    add_library(liblua INTERFACE)
    target_include_directories(liblua INTERFACE  "${CMAKE_CURRENT_SOURCE_DIR}/luajit/src/")

    if (ANDROID)
        if (ARCHITECTURE_ARM32)
            target_link_libraries(liblua INTERFACE
                "${CMAKE_CURRENT_SOURCE_DIR}/luajit-android/armv7a/libluajit.a")

            set(LUA_DLL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/luajit-android/armv7a/libluajit.so")
        elseif (ARCHITECTURE_AARCH64)
            target_link_libraries(liblua INTERFACE
                    "${CMAKE_CURRENT_SOURCE_DIR}/luajit-android/aarch64/libluajit.a")

            set(LUA_DLL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/luajit-android/aarch64/libluajit.so")
        else()
            target_link_libraries(liblua INTERFACE
                    "${CMAKE_CURRENT_SOURCE_DIR}/luajit-android/x86_64/libluajit.a")

            set(LUA_DLL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/luajit-android/x86_64/libluajit.so")
		endif()
    elseif (MSVC)
        ExternalProject_Add(luaorg
                SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/luajit"
                SOURCE_SUBDIR src
                CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/luajit ${LUA_BINARY_DIR}
                BINARY_DIR "${LUA_BINARY_DIR}/src"
                BUILD_COMMAND "${LUA_BINARY_DIR}/src/msvcbuild.bat"
                INSTALL_COMMAND ""
                BUILD_BYPRODUCTS "${LUA_BINARY_DIR}/src/lua51.dll"
                BUILD_BYPRODUCTS "${LUA_BINARY_DIR}/src/lua51.lib")

        set(LUA_DLL_PATH "${LUA_BINARY_DIR}/src/lua51.dll" PARENT_SCOPE)

        target_link_libraries(liblua INTERFACE
                "${LUA_BINARY_DIR}/src/lua51.lib")

        add_dependencies(liblua luaorg)
    else()
        ExternalProject_Add(luaorg
            SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/luajit"
            SOURCE_SUBDIR src
            CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/luajit ${LUA_BINARY_DIR}
            BINARY_DIR "${LUA_BINARY_DIR}/src"
            BUILD_COMMAND cd ${LUA_BINARY_DIR} && export MACOSX_DEPLOYMENT_TARGET=10.12 && make
            INSTALL_COMMAND ""
            BUILD_BYPRODUCTS "${LUA_BINARY_DIR}/src/libluajit.so"
            BUILD_BYPRODUCTS "${LUA_BINARY_DIR}/src/libluajit.a"
        )

        set(LUA_DLL_PATH "${LUA_BINARY_DIR}/src/libluajit.so" PARENT_SCOPE)

        target_link_libraries(liblua INTERFACE "${LUA_BINARY_DIR}/src/libluajit.a")

        add_dependencies(liblua luaorg)
    endif()
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

if (NOT ARCHITECTURE_ARM32)
    add_subdirectory(dynarmic)
endif()

if (ANDROID)
    add_subdirectory(lunasvg)
endif()

option(YAML_CPP_BUILD_TESTS "Enable YAML cpp testing" OFF)
add_subdirectory(yaml-cpp)

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
add_library(xxHash STATIC xxHash/xxhash.c)
target_include_directories(xxHash PUBLIC xxHash)

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

# Forcefully disable AAudio (we build with API target 21 minimum)
set (USE_AAUDIO FALSE CACHE INTERNAL "" FORCE)

add_subdirectory(cubeb)


# FFMPEG
add_subdirectory(ffmpeg)
if (APPLE)
    target_link_libraries(ffmpeg INTERFACE "-framework CoreVideo" "iconv")
endif()

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

# libfat
add_subdirectory(libfat)

# mbedtls
set(ENABLE_TESTING OFF CACHE BOOL "Build mbed TLS tests." FORCE)
add_subdirectory(mbedtls)

# SQLite3
add_library(sqlite3
    sqlite3/sqlite3.c
    sqlite3/sqlite3.h
    sqlite3/sqlite3ext.h)

target_include_directories(sqlite3 PUBLIC sqlite3)

# miniBae. Later in driver, we will implement the full thing hopefully :D
file(GLOB MINIBAE_SHARED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/miniBAE/minibae/BAE_Source/Common/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/miniBAE/minibae/BAE_Source/Common/*.h
)

set(MINIBAE_INTERNAL_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/miniBAE/minibae/BAE_Source/Common/
    ${CMAKE_CURRENT_SOURCE_DIR}/miniBAE/minibae/BAE_Source/Platform/
    CACHE INTERNAL "MiniBAE internal include directories")

set(MINIBAE_PUBLIC_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/miniBAE/minibae/
    CACHE INTERNAL "MiniBAE public include directories")

set(MINIBAE_SHARED_SOURCES ${MINIBAE_SHARED_SOURCES} CACHE INTERNAL "MiniBAE source code")

# TinySoundFont
set (TSF_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "TSF include path")

# SDL2
# Can't believe in 2022 the official CMake can't still ship a FindSDL2 with the program...
if (NOT ANDROID)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/sdl2-cmake-scripts")

    if(APPLE)
        set(SDL2_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sdl2/macos" CACHE PATH "Where SDL2 is located" FORCE)
    elseif(WIN32)
        set(SDL2_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sdl2/Windows" CACHE PATH "Where SDL2 is located" FORCE)
        set(SDL2_INCLUDE_DIR "${SDL2_PATH}/include" CACHE PATH "Where the SDL2 include files can be found" FORCE)
    endif()

    find_package(SDL2 REQUIRED)
    add_library(SDL2 INTERFACE)

    target_include_directories(SDL2 INTERFACE "${SDL2_INCLUDE_DIR}")
    target_link_libraries(SDL2 INTERFACE "${SDL2_LIBRARY}")
endif()

# libuv
set(LIBUV_BUILD_TESTS "Build test for libuv" CACHE INTERNAL OFF)
set(LIBUV_BUILD_BENCH "Build bench for libuv" CACHE INTERNAL OFF)

add_subdirectory(libuv)

# UPNP

option (UPNPC_BUILD_SHARED "Build shared library" FALSE)
option (UPNPC_BUILD_TESTS "Build test executables" FALSE)
option (UPNPC_BUILD_SAMPLE "Build sample executables" FALSE)
option (UPNPC_NO_INSTALL "Disable installation" TRUE)

add_subdirectory(miniupnp/miniupnpc)

set (capstone_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/capstone/include" PARENT_SCOPE)
set (YAML_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/yaml-cpp/include" PARENT_SCOPE)
set (LIBCXXABI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libcxxabi/include" PARENT_SCOPE)
set (FRSML_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/FRSML/include" PARENT_SCOPE)
set (GLM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/glm" PARENT_SCOPE)
