add_library(common
    include/common/arghandler.h
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
    include/common/virtualmem.h
    include/common/wildcard.h
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
    src/virtualmem.cpp
    src/wildcard.cpp
)

target_include_directories(common PUBLIC include)
target_link_libraries(common PUBLIC fmt miniz spdlog)
target_link_libraries(common PRIVATE pugixml)

if (UNIX OR APPLE)
    set_property(TARGET common PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/common/configure.h)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/common/version.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/include/common/version.h)