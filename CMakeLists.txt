cmake_minimum_required(VERSION 3.8)

project (EKA2L1)

# Clang 14 is required to compile nicely with C++ 20
set (CMAKE_CXX_STANDARD 20)

# iOS detection. When building with cmake/ios.toolchain.cmake, CMAKE_SYSTEM_NAME
# is set to "iOS" before project() runs, so this flag reflects that.
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(EKA2L1_IOS ON)
else()
    set(EKA2L1_IOS OFF)
endif()

set(EKA2L1_IOS_DEPLOYMENT_TARGET "16.0" CACHE STRING "Minimum iOS version to target")

option(EKA2L1_BUILD_TOOLS "Build EKA2L1's tool" ON)
option(EKA2L1_BUILD_TESTS "Build EKA2L1's tests" ON)
option(EKA2L1_ENABLE_SCRIPTING_ABILITY "Enable to script with Python or Lua" ON)
option(EKA2L1_SCRIPTING_LUA "Use the LuaJIT runtime for scripting. When OFF, only the built-in native patches are compiled (no runtime script loading)." ON)
option(EKA2L1_ENABLE_UNEXPECTED_EXCEPTION_HANDLER "Enable EKA2L1 to dump unexpected exception" OFF)
option(EKA2L1_BUILD_VULKAN_BACKEND "Build Vulkan backend" OFF)
option(EKA2L1_DEPLOY_DMG "Deploy EKA2L1 as .dmg" OFF)
option(EKA2L1_BUILD_PATCH "Enable building Symbian's DLL patches using Symbian SDK" OFF)
option(EKA2L1_ENABLE_DISCORD_RICH_PRESENCE "Enable support for Discord Rich Presence" OFF)
option(EKA2L1_ENABLE_QT_CAMERA "Build the desktop camera backend on Qt Multimedia" ON)

# Runtime sanitizer for debugging builds: "address" (ASan, memory errors) or
# "thread" (TSan, data races and lock-order inversions). Empty disables both.
# They are mutually exclusive -- the shadow memory layouts conflict.
set(EKA2L1_SANITIZER "" CACHE STRING "Runtime sanitizer to build with: address, thread, or empty")
set_property(CACHE EKA2L1_SANITIZER PROPERTY STRINGS "" "address" "thread")

if (EKA2L1_SANITIZER AND NOT EKA2L1_SANITIZER MATCHES "^(address|thread)$")
    message(FATAL_ERROR "EKA2L1_SANITIZER must be 'address', 'thread', or empty (got '${EKA2L1_SANITIZER}')")
endif()

# Options the iOS port does not support. They are overridden silently so a user
# passing -DEKA2L1_BUILD_TESTS=ON for another platform does not have to
# special-case iOS in their scripts.
if (EKA2L1_IOS)
    set(EKA2L1_IOS_SIMULATOR OFF)
    if (CMAKE_OSX_SYSROOT MATCHES "[Ii][Pp]honeSimulator")
        set(EKA2L1_IOS_SIMULATOR ON)
    endif()
    # Dynarmic JIT on iOS — one option, one compile macro, simulator and
    # device alike. Defaults: ON for simulator builds (the sim process runs on
    # macOS and can always JIT), OFF for device builds. On device only
    # sideload/unsigned-IPA builds should turn this on: executable memory on a
    # jailed iPhone needs the process to be debuggable (re-signed with
    # get-task-allow, JIT enabled via debugger or a JIT enabler) — App Store /
    # TestFlight processes can never flip pages to RX, so distribution builds
    # keep this OFF and ship without any JIT code (build_ios.sh wires the
    # per-flavor defaults). Availability is still probed at runtime
    # (arm::host_can_jit()); when the probe fails the emulator falls back to
    # the dyncom interpreter.
    # A sanitizer cannot see into JIT-emitted code, and TSan in particular
    # reports the JIT's own code buffers as races. Default the JIT off when a
    # sanitizer is on so the run goes through the (fully instrumented) dyncom
    # interpreter instead.
    if (EKA2L1_SANITIZER)
        set(EKA2L1_IOS_DYNARMIC_DEFAULT OFF)
    else()
        set(EKA2L1_IOS_DYNARMIC_DEFAULT ${EKA2L1_IOS_SIMULATOR})
    endif()

    option(EKA2L1_IOS_DYNARMIC "Compile the dynarmic JIT into iOS builds (device: sideload/unsigned IPA only)" ${EKA2L1_IOS_DYNARMIC_DEFAULT})

    if (EKA2L1_IOS_DYNARMIC)
        add_compile_definitions(EKA2L1_IOS_DYNARMIC=1)
        # boost still uses std::unary_function, removed from libc++'s C++17 mode.
        add_compile_definitions(_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION=1)
        message(STATUS "iOS build: Dynarmic JIT compiled in")
    endif()

    set(EKA2L1_BUILD_TOOLS OFF CACHE BOOL "Disabled on iOS" FORCE)
    set(EKA2L1_BUILD_TESTS OFF CACHE BOOL "Disabled on iOS" FORCE)
    # iOS keeps the scripting engine (for the built-in native game patches) but
    # cannot use the LuaJIT runtime: JIT/FFI callbacks need writable-executable
    # memory, which iOS forbids on non-jailbroken devices. Build native-only.
    set(EKA2L1_ENABLE_SCRIPTING_ABILITY ON CACHE BOOL "Native patches only on iOS" FORCE)
    set(EKA2L1_SCRIPTING_LUA OFF CACHE BOOL "No LuaJIT on iOS" FORCE)
    set(EKA2L1_BUILD_VULKAN_BACKEND OFF CACHE BOOL "Disabled on iOS" FORCE)
    set(EKA2L1_BUILD_PATCH OFF CACHE BOOL "Disabled on iOS" FORCE)
    set(EKA2L1_DEPLOY_DMG OFF CACHE BOOL "Disabled on iOS" FORCE)
    set(EKA2L1_ENABLE_DISCORD_RICH_PRESENCE OFF CACHE BOOL "Disabled on iOS" FORCE)
    message(STATUS "iOS build: tools/tests/vulkan/patch/dmg/discord forced OFF; scripting is native-patch only (no LuaJIT)")
endif()


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
    add_definitions(-D_DISABLE_VECTOR_ANNOTATION)
endif(MSVC)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Limit to C-family languages so that swiftc (enabled for the iOS target)
    # does not reject the clang-only flag.
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-Wno-error>)
endif()

if (EKA2L1_SANITIZER)
    if (CMAKE_GENERATOR STREQUAL "Xcode")
        # Let Xcode drive it: the build setting instruments C/ObjC/C++/Swift and
        # links the runtime in one go, which hand-rolled flags on a mixed-language
        # target get wrong (swiftc spells the flag -sanitize=, not -fsanitize=).
        if (EKA2L1_SANITIZER STREQUAL "address")
            set(CMAKE_XCODE_ATTRIBUTE_ENABLE_ADDRESS_SANITIZER "YES")
        else()
            set(CMAKE_XCODE_ATTRIBUTE_ENABLE_THREAD_SANITIZER "YES")
        endif()
    else()
        add_compile_options($<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-fsanitize=${EKA2L1_SANITIZER}>)
        add_compile_options($<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-fno-omit-frame-pointer>)
        add_link_options(-fsanitize=${EKA2L1_SANITIZER})
    endif()
    message(STATUS "Sanitizer enabled: ${EKA2L1_SANITIZER}")
endif()


if (APPLE)
    # The bundled boost (pulled in by dynarmic) still uses std::unary_function,
    # which libc++ no longer declares in C++17 and later modes.
    add_compile_definitions(_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION=1)
endif()

if (EKA2L1_BUILD_VULKAN_BACKEND)
    set (BUILD_WITH_VULKAN 1)
else()
    set (BUILD_WITH_VULKAN 0)
endif()

if (EKA2L1_ENABLE_SCRIPTING_ABILITY)
    set (ENABLE_SCRIPTING 1)
endif(EKA2L1_ENABLE_SCRIPTING_ABILITY)

if (EKA2L1_ENABLE_SCRIPTING_ABILITY AND EKA2L1_SCRIPTING_LUA)
    set (ENABLE_SCRIPTING_LUA 1)
endif()

if (CI)
    set (BUILD_FOR_USER 1)
endif (CI)

set (ENABLE_SEH_HANDLER 0)

if (EKA2L1_ENABLE_UNEXPECTED_EXCEPTION_HANDLER)
    set(ENABLE_SEH_HANDLER 1)
endif (EKA2L1_ENABLE_UNEXPECTED_EXCEPTION_HANDLER)

if (EKA2L1_ENABLE_DISCORD_RICH_PRESENCE AND NOT ANDROID)
    set(ENABLE_DISCORD_RICH_PRESENCE 1)
else()
    set(ENABLE_DISCORD_RICH_PRESENCE 0)
endif()

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
