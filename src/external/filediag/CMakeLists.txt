# Based on Premake5.lua in build directory

if (WIN32)
    set (NATIVEFILEDIAG_PLATFORM_SOURCE
        src/nfd_win.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set (NATIVEFILEDIAG_PLATFORM_SOURCE
        src/nfd_cocoa.m)
else()
    set(NATIVEFILEDIAG_PLATFORM_SOURCE
        src/nfd_gtk.c)
endif()

add_library(nativefilediag
    src/nfd_common.c
    src/nfd_common.h
    ${NATIVEFILEDIAG_PLATFORM_SOURCE})

target_include_directories(nativefilediag PUBLIC src/include)

if (UNIX AND NOT APPLE)
    # Need GTK3
    find_package(GTK3 REQUIRED)

    add_library(gtk3 INTERFACE)
    target_include_directories(gtk3 INTERFACE ${GTK3_INCLUDE_DIRS})
    target_link_libraries(gtk3 INTERFACE ${GTK3_LIBRARIES})

    target_link_libraries(nativefilediag PRIVATE gtk3)
endif ()
