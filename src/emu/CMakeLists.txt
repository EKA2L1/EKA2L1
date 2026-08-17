if (WIN32)
    add_definitions(-DNOMINMAX)
    message("NOMINMAX")

    if (MSVC AND ENABLE_SEH_HANDLER)
        # Replace SEH flag if there is one
        string(REGEX REPLACE "\/(EH).+" "\/EHa" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHa")
    endif ()
endif ()

# Many of our code are optimized to utilize offsetof.
# But it complains a lot with non-POD type, that's not what we want, so disable the warn flag.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    # CMAKE_OSX_DEPLOYMENT_TARGET also applies to iOS builds, and the iOS
    # toolchain sets it from EKA2L1_IOS_DEPLOYMENT_TARGET. Do not overwrite it
    # there with the macOS default.
    if (NOT EKA2L1_IOS)
        if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
            set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X version to target for deployment" FORCE)
        endif ()
    endif ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-invalid-offsetof")
endif ()

add_subdirectory(bridge)
add_subdirectory(common)
add_subdirectory(config)
add_subdirectory(cpu)
add_subdirectory(disasm)
add_subdirectory(dispatch)
add_subdirectory(drivers)
add_subdirectory(gdbstub)
add_subdirectory(j2me)
add_subdirectory(kernel)
add_subdirectory(ldd)
add_subdirectory(loader)
add_subdirectory(mem)
add_subdirectory(package)
add_subdirectory(services)
add_subdirectory(system)
add_subdirectory(utils)
add_subdirectory(vfs)

if (ANDROID)
    include_directories(android/app/src/main)
    add_subdirectory(android/app/src/main/cpp)
elseif (EKA2L1_IOS)
    add_subdirectory(ios)
else()
    add_subdirectory(qt)
endif()

if (EKA2L1_ENABLE_SCRIPTING_ABILITY)
    add_subdirectory(scripting)
endif ()
