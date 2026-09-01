include(ExternalProject)
include(CheckSymbolExists)

# Several of the vendored projects still declare a cmake_minimum_required below
# 3.5, which CMake 4 refuses outright. They are not ours to change -- two of them
# are submodules of dynarmic -- so the floor is relaxed for this subtree only,
# rather than making every caller pass the flag.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

add_library(miniz miniz/miniz.h miniz/miniz.c)
target_include_directories(miniz PUBLIC miniz)

# libarchive, used to read device dumps packed as .7z. It is here rather than a bare 7z decoder so that
# adding zip/tar/rar support later is a matter of flipping options instead of another vendored codebase.
#
# Everything it can autodetect is switched off on purpose. libarchive's CMake probes the *host* for
# liblzma, zlib, bzip2, OpenSSL and friends; on an iOS or Android build those probes find the macOS or
# Homebrew dylibs and the link step fails (same trap freetype falls into further down). So: every backend
# is disabled, and LZMA - the one 7z actually needs - is built from the xz submodule and handed to
# libarchive below. To add another compression method later, build it here the same way rather than
# turning its ENABLE_ switch back on.
set(EKA2L1_SAVED_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)

set(XZ_NLS OFF CACHE BOOL "" FORCE)
set(XZ_DOC OFF CACHE BOOL "" FORCE)
set(XZ_TOOL_XZ OFF CACHE BOOL "" FORCE)
set(XZ_TOOL_XZDEC OFF CACHE BOOL "" FORCE)
set(XZ_TOOL_LZMADEC OFF CACHE BOOL "" FORCE)
set(XZ_TOOL_LZMAINFO OFF CACHE BOOL "" FORCE)
set(XZ_TOOL_SCRIPTS OFF CACHE BOOL "" FORCE)
set(XZ_TOOL_SYMLINKS OFF CACHE BOOL "" FORCE)
set(XZ_TOOL_SYMLINKS_LZMA OFF CACHE BOOL "" FORCE)
# Nothing here writes archives, and the threaded coders drag in a pthread configuration we do not want
# on every platform we ship to.
set(XZ_THREADS no CACHE STRING "" FORCE)

add_subdirectory(xz EXCLUDE_FROM_ALL)

# Answer libarchive's FIND_PACKAGE(LibLZMA) with the xz target above instead of letting it search.
# Every one of these is a short-circuit for a specific step of the stock FindLibLZMA: a cached
# LIBLZMA_INCLUDE_DIR skips its find_path (which on this machine happily returns Homebrew's headers), a
# set LIBLZMA_LIBRARY skips its find_library (which returns the SDK's liblzma.tbd - a /usr/lib dylib
# that must not end up in a shipped iOS binary), and the three HAS_ results skip check_library_exists,
# which cannot link a target that has not been built yet. LIBLZMA_LIBRARY naming a target rather than a
# file is deliberate: CMake resolves it at generate time, which is what makes libarchive depend on it.
# Dropping a FindLibLZMA.cmake of our own in CMAKE_MODULE_PATH does not work - libarchive overwrites
# that variable wholesale on the third line of its CMakeLists.
set(LIBLZMA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/xz/src/liblzma/api" CACHE PATH "" FORCE)
set(LIBLZMA_LIBRARY liblzma)
set(LIBLZMA_HAS_AUTO_DECODER TRUE CACHE INTERNAL "")
set(LIBLZMA_HAS_EASY_ENCODER TRUE CACHE INTERNAL "")
set(LIBLZMA_HAS_LZMA_PRESET TRUE CACHE INTERNAL "")

set(ENABLE_LZMA ON CACHE BOOL "" FORCE)

# Nothing here uses libarchive's regex matching, and letting it search costs a
# build: on MSVC no POSIX regex library is found, and libarchive's fall-through
# for that case fatals with "libgcc not found" instead of moving on.
set(POSIX_REGEX_LIB "NONE" CACHE STRING "" FORCE)
foreach (libarchive_backend MBEDTLS NETTLE OPENSSL LIBB2 LZ4 LZO ZSTD ZLIB BZip2 LIBXML2 EXPAT
        WIN32_XMLLITE PCREPOSIX PCRE2POSIX LIBGCC CNG ICONV XATTR ACL)
    set(ENABLE_${libarchive_backend} OFF CACHE BOOL "" FORCE)
endforeach()

foreach (libarchive_extra TAR CPIO CAT UNZIP TEST INSTALL WERROR)
    set(ENABLE_${libarchive_extra} OFF CACHE BOOL "" FORCE)
endforeach()

# libarchive decides which platform functions exist with CHECK_FUNCTION_EXISTS, which only tells the
# truth if the probe is actually linked. The iOS toolchain compiles try_compile projects as static
# libraries (see ENABLE_STRICT_TRY_COMPILE in cmake/ios.toolchain.cmake), so an unresolved symbol is never
# noticed and every probe answers yes - _fseeki64, futimesat, close_range, lzma_stream_encoder_mt and
# friends included, none of which exist here. libarchive then compiles the code behind them.
#
# So make this subtree link its probes, and put the setting back afterwards: the rest of the tree is
# configured and working against the lenient answers, and flipping it globally would re-answer every
# other project's probes too. Seeding the known-wrong results by hand instead was tried and rejected -
# comparing the two generated config.h files showed the list is longer and less obvious than it looks,
# and it would have to be revisited every time libarchive adds a probe.
set(EKA2L1_SAVED_TRY_COMPILE_TARGET_TYPE ${CMAKE_TRY_COMPILE_TARGET_TYPE})
set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)

add_subdirectory(libarchive EXCLUDE_FROM_ALL)

set(CMAKE_TRY_COMPILE_TARGET_TYPE ${EKA2L1_SAVED_TRY_COMPILE_TARGET_TYPE})

set(BUILD_SHARED_LIBS ${EKA2L1_SAVED_BUILD_SHARED_LIBS})

if (UNIX OR APPLE)
    set_property(TARGET liblzma PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET archive_static PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

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

if (EKA2L1_ENABLE_SCRIPTING_ABILITY AND EKA2L1_SCRIPTING_LUA)
    set(LUA_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/luaorg)

    add_library(liblua INTERFACE)

    if (ANDROID)
        target_include_directories(liblua INTERFACE  "${CMAKE_CURRENT_SOURCE_DIR}/luajit-android-src/src/")

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
    else()
        target_include_directories(liblua INTERFACE  "${CMAKE_CURRENT_SOURCE_DIR}/luajit/src/")

        option(LUAJIT_DIR "" ON)
        set(LUAJIT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/luajit")

        add_subdirectory(luajit-cmake)

        # Fix up a bit
        target_include_directories(buildvm PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/luajit-cmake")

        target_link_libraries(liblua INTERFACE luajit::lib luajit::header)
        target_include_directories(liblua INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/luajit-cmake")
    endif()
endif()

# SPDLOG - Build examples off
option (SPDLOG_BUILD_EXAMPLES "Build spdlog Examples" OFF) 
option (SPDLOG_BUILD_BENCH "Build spdlog benchmarks" OFF)
option (SPDLOG_FMT_EXTERNAL "Use external fmt library instead of bundled" ON)

add_subdirectory(spdlog)

# GLM
option (GLM_TEST_ENABLE "Enable glm tests" OFF)
add_subdirectory(glm)

# Dynarmic: disable tests
option(DYNARMIC_TESTS "Enable dynarmic tests" OFF)

if (NOT ARCHITECTURE_ARM32 AND (NOT EKA2L1_IOS OR EKA2L1_IOS_DYNARMIC))
    # On iOS, dynarmic is only compiled when EKA2L1_IOS_DYNARMIC is set
    # (simulator default, sideload-only device builds); App Store / TestFlight
    # builds omit it entirely.
    add_subdirectory(dynarmic)
endif()

# lunasvg 3.5.0 (upstream submodule) bundles the plutovg rasterizer. Turn off
# its example executables and skip scanning OS font directories: Symbian app
# icons carry no text, so we never need system fonts, and the scan is pointless
# work inside a mobile sandbox.
set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "Disable lunasvg examples" FORCE)
set(PLUTOVG_BUILD_EXAMPLES OFF CACHE BOOL "Disable plutovg examples" FORCE)
set(LUNASVG_DISABLE_LOAD_SYSTEM_FONTS ON CACHE BOOL "Skip OS font directories" FORCE)
add_subdirectory(lunasvg)

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

if (NOT EKA2L1_IOS)
    # iOS uses a native AURemoteIO audio_driver (see
    # src/emu/drivers/.../audiounit_ios). cubeb's bundled cubeb_audiounit.cpp
    # targets macOS CoreAudio HAL and doesn't cleanly compile on the iOS SDK.
    add_subdirectory(cubeb)
endif()


# FFMPEG
if (EKA2L1_IOS)
    option(EKA2L1_IOS_ENABLE_FFMPEG "Link iOS builds against a prebuilt bundled FFmpeg output" OFF)
    # The ffmpeg submodule ships prebuilt libraries for the desktop platforms and
    # Android, but not for iOS: its ios-build.sh produces them into ios/<slice>.
    if (EKA2L1_IOS_SIMULATOR)
        set(EKA2L1_IOS_FFMPEG_SLICE "simulator")
    else()
        set(EKA2L1_IOS_FFMPEG_SLICE "device")
    endif()

    set(EKA2L1_IOS_FFMPEG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/ios/${EKA2L1_IOS_FFMPEG_SLICE}"
        CACHE PATH "Path to an iOS FFmpeg install prefix with include/ and lib/")

    if (EKA2L1_IOS_ENABLE_FFMPEG)
        if (NOT EXISTS "${EKA2L1_IOS_FFMPEG_ROOT}/lib/libavcodec.a")
            message(FATAL_ERROR
                "An iOS build needs ffmpeg: run `./ios-build.sh ${EKA2L1_IOS_FFMPEG_SLICE}` "
                "in src/external/ffmpeg.")
        endif()

        add_library(ffmpeg INTERFACE)
        target_include_directories(ffmpeg INTERFACE
            "${EKA2L1_IOS_FFMPEG_ROOT}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/include")
        target_link_libraries(ffmpeg INTERFACE
            "${EKA2L1_IOS_FFMPEG_ROOT}/lib/libavformat.a"
            "${EKA2L1_IOS_FFMPEG_ROOT}/lib/libavcodec.a"
            "${EKA2L1_IOS_FFMPEG_ROOT}/lib/libswscale.a"
            "${EKA2L1_IOS_FFMPEG_ROOT}/lib/libavutil.a"
            "${EKA2L1_IOS_FFMPEG_ROOT}/lib/libswresample.a"
            "z" "iconv"
            "-framework CoreVideo"
            "-framework VideoToolbox"
            "-framework CoreMedia")
        set(EKA2L1_HAVE_FFMPEG ON CACHE INTERNAL "FFmpeg is available for this build" FORCE)
    else()
        set(EKA2L1_HAVE_FFMPEG OFF CACHE INTERNAL "FFmpeg is available for this build" FORCE)
    endif()
else()
    set(EKA2L1_HAVE_FFMPEG ON CACHE INTERNAL "FFmpeg is available for this build" FORCE)
    if (APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        # The prebuilt libraries in the submodule are x86_64 only; its own
        # macos_arm64-build.sh produces the arm64 set from the same source.
        set(FFMPEG_ARM64_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/macos/arm64/lib")
        if (NOT EXISTS "${FFMPEG_ARM64_LIB_DIR}/libavcodec.a")
            message(FATAL_ERROR "An Apple Silicon build needs ffmpeg: run macos_arm64-build.sh in src/external/ffmpeg")
        endif()

        add_library(ffmpeg INTERFACE)
        target_include_directories(ffmpeg INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/include")
        target_link_libraries(ffmpeg INTERFACE
                "${FFMPEG_ARM64_LIB_DIR}/libavformat.a"
                "${FFMPEG_ARM64_LIB_DIR}/libavcodec.a"
                "${FFMPEG_ARM64_LIB_DIR}/libswscale.a"
                "${FFMPEG_ARM64_LIB_DIR}/libavutil.a"
                "${FFMPEG_ARM64_LIB_DIR}/libswresample.a"
                "z" "-framework CoreVideo" "iconv")
    else()
        add_subdirectory(ffmpeg)
        if (APPLE)
            target_link_libraries(ffmpeg INTERFACE "-framework CoreVideo" "iconv")
        endif()
    endif()
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
# Nothing here links mbedcrypto/mbedtls/mbedx509 -- libarchive's mbedtls backend is
# forced off above -- so keep it out of "all" like the other vendored libraries that
# are only built on demand. The settings below keep it buildable if something ever
# does link it: mbedtls builds its test helpers when either ENABLE_TESTING or
# ENABLE_PROGRAMS is on, and its -Werror does not survive GCC 14+/Clang 16.
set(ENABLE_TESTING OFF CACHE BOOL "Build mbed TLS tests." FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "Build mbed TLS programs." FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "Compiler warnings treated as errors" FORCE)
add_subdirectory(mbedtls EXCLUDE_FROM_ALL)

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
if (NOT ANDROID AND NOT EKA2L1_IOS)
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
option(LIBUV_BUILD_TESTS "Build test for libuv" OFF)
option(LIBUV_BUILD_BENCH "Build bench for libuv" OFF)
option(BUILD_UVW_LIBS "Build UVW libraries" ON)
option(FETCH_LIBUV "Fetch the libuv repo using CMake FetchContent facility" ON)
set (BUILD_TESTING OFF CACHE INTERNAL "Enable testing with ctest." FORCE)

add_subdirectory(uvw)
add_subdirectory(uvlooper)

# UPNP

option (UPNPC_BUILD_SHARED "Build shared library" FALSE)
option (UPNPC_BUILD_TESTS "Build test executables" FALSE)
option (UPNPC_BUILD_SAMPLE "Build sample executables" FALSE)
option (UPNPC_NO_INSTALL "Disable installation" TRUE)

add_subdirectory(miniupnp/miniupnpc)

# libtess2

add_library(libtess2
    libtess2/Source/bucketalloc.c
    libtess2/Source/bucketalloc.h
    libtess2/Source/dict.c
    libtess2/Source/dict.h
    libtess2/Source/geom.c
    libtess2/Source/geom.h
    libtess2/Source/mesh.c
    libtess2/Source/mesh.h
    libtess2/Source/priorityq.c
    libtess2/Source/priorityq.h
    libtess2/Source/sweep.c
    libtess2/Source/sweep.h
    libtess2/Source/tess.c
    libtess2/Source/tess.h)

target_include_directories(libtess2 PUBLIC libtess2/Include)

# RectangleBinPack
add_subdirectory(RectangleBinPack)

# Re2
option(RE2_BUILD_TESTING "enable testing for RE2" OFF)
add_subdirectory(re2)

# Discord-RPC
if(ENABLE_DISCORD_RICH_PRESENCE)
	add_subdirectory(discord-rpc-build)
endif()

add_library(thread-pool INTERFACE)
target_include_directories(thread-pool INTERFACE thread-pool/include)

add_definitions(-DFT_CONFIG_OPTION_SUBPIXEL_RENDERING)

set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_ENABLE_ERROR_STRINGS ON CACHE BOOL "" FORCE)

if (EKA2L1_IOS)
    # freetype's CMake autoprobe would otherwise pick up Homebrew's macOS
    # libpng / zlib / bzip2 / brotli, which link as macOS dylibs and fail
    # the iOS link step. The Symbian content we render through freetype
    # does not need these compression backends.
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
endif()

add_subdirectory(freetype)

set (capstone_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/capstone/include" PARENT_SCOPE)
set (YAML_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/yaml-cpp/include" PARENT_SCOPE)
set (LIBCXXABI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libcxxabi/include" PARENT_SCOPE)
set (FRSML_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/FRSML/include" PARENT_SCOPE)
set (GLM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/glm" PARENT_SCOPE)
