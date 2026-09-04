if (WIN32)
    set(CUSTOM_COMMON_SOURCE
            src/watcher_win32.h
            src/watcher_win32.cpp)
elseif (UNIX AND NOT APPLE)
    set(CUSTOM_COMMON_SOURCE
            src/watcher_unix.h
            src/watcher_unix.cpp)
else ()
    set(CUSTOM_COMMON_SOURCE
            src/watcher_null.h
            src/watcher_null.cpp)
endif ()

add_library(common
        include/common/applauncher.h
        include/common/archive.h
        include/common/arghandler.h
        include/common/atomic.h
        include/common/advstream.h
        include/common/allocator.h
        include/common/algorithm.h
        include/common/armcommon.h
        include/common/armemitter.h
        include/common/bitfield.h
        include/common/bitmap.h
        include/common/buffer.h
        include/common/bytepair.h
        include/common/bytes.h
        include/common/chunkyseri.h
        include/common/codeblock.h
        include/common/color.h
        include/common/crypt.h
        include/common/cpudetect.h
        include/common/cvt.h
        include/common/dictcomp.h
        include/common/dynamicfile.h
        include/common/fileutils.h
        include/common/flate.h
        include/common/hash.h
        include/common/ini.h
        include/common/linked.h
        include/common/language.h
        include/common/localizer.h
        include/common/log.h
        include/common/map.h
        include/common/paint.h
        include/common/path.h
        include/common/platform.h
        include/common/queue.h
        include/common/random.h
        include/common/raw_bind.h
        include/common/resource.h
        include/common/runlen.h
        include/common/svg.h
        include/common/sync.h
        include/common/thread.h
        include/common/time.h
        include/common/types.h
        include/common/unicode.h
        include/common/url.h
        include/common/upnp.h
        include/common/region.h
        include/common/virtualmem.h
        include/common/watcher.h
        include/common/wildcard.h
        src/archive.cpp
        src/atomic.cpp
        src/arghandler.cpp
        src/armemitter.cpp
        src/allocator.cpp
        src/algorithm.cpp
        src/arm_cpudetect.cpp
        src/bytepair.cpp
        src/bytes.cpp
        src/chunkyseri.cpp
        src/cvt.cpp
        src/color.cpp
        src/crypt.cpp
        src/dictcomp.cpp
        src/dynamicfile.cpp
        src/fileutils.cpp
        src/flate.cpp
        src/hash.cpp
        src/ini.cpp
        src/language.cpp
        src/localizer.cpp
        src/log.cpp
        src/paint.cpp
        src/path.cpp
        src/random.cpp
        src/runlen.cpp
        src/svg.cpp
        src/sync.cpp
        src/thread.cpp
        src/time.cpp
        src/types.cpp
        src/unicode.cpp
        src/url.cpp
        src/upnp.cpp
        src/region.cpp
        src/virtualmem.cpp
        src/watcher.cpp
        src/wildcard.cpp
        ${CUSTOM_COMMON_SOURCE}
        )

if (ANDROID)
    target_sources(common PRIVATE
        include/common/android/audio.h
        include/common/android/storage.h
        include/common/android/jniutils.h
        include/common/android/contenturi.h
        src/android/audio.cpp
        src/android/applauncher.cpp
        src/android/storage.cpp
        src/android/jniutils.cpp
        )
elseif (EKA2L1_IOS)
    target_sources(common PRIVATE
        src/ios/applauncher.mm
        )
    target_link_libraries(common PRIVATE "-framework UIKit" "-framework Foundation")
endif()

if (APPLE)
    target_link_libraries(common PRIVATE "-framework CoreFoundation")
endif()

set(COMMON_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
file(MAKE_DIRECTORY "${COMMON_GENERATED_INCLUDE_DIR}/common")

target_include_directories(common PUBLIC
        "${COMMON_GENERATED_INCLUDE_DIR}"
        include)
target_link_libraries(common PUBLIC fmt spdlog zlibstatic)
target_link_libraries(common PRIVATE archive_static pugixml miniupnpc::miniupnpc re2::re2)

if (WIN32)
    # libarchive names its temporary files with BCryptGenRandom whatever ENABLE_CNG says, but only links
    # bcrypt when that option is on. It cannot be added to its own target from here (CMP0079).
    target_link_libraries(common PRIVATE bcrypt)
endif()
target_link_libraries(common PRIVATE miniupnpc::miniupnpc)

if (UNIX OR APPLE)
    set_property(TARGET common PROPERTY POSITION_INDEPENDENT_CODE ON)
    if (NOT ANDROID)
        target_link_libraries(common PRIVATE pthread)
    else()
        target_link_libraries(common PRIVATE android)
    endif()
endif ()

execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/common/configure.h.in
        ${COMMON_GENERATED_INCLUDE_DIR}/common/configure.h)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/common/version.h.in
        ${COMMON_GENERATED_INCLUDE_DIR}/common/version.h)
