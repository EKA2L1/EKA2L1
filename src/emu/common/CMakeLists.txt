add_library(common
    include/common/advstream.h
    include/common/algorithm.h
    include/common/buffer.h
    include/common/bytepair.h
    include/common/chunkyseri.h
    include/common/cpudetect.h
    include/common/cvt.h
    include/common/data_displayer.h
    include/common/fileutils.h
    include/common/flate.h
    include/common/hash.h
    include/common/log.h
    include/common/path.h
    include/common/platform.h
    include/common/queue.h
    include/common/random.h
    include/common/raw_bind.h
    include/common/resource.h
    include/common/types.h
    include/common/virtualmem.h
    src/algorithm.cpp
    src/arm_cpudetect.cpp
    src/bytepair.cpp
    src/chunkyseri.cpp
    src/cvt.cpp
    src/data_displayer.cpp
    src/fileutils.cpp
    src/flate.cpp
    src/hash.cpp
    src/log.cpp
    src/path.cpp
    src/random.cpp
    src/types.cpp
    src/virtualmem.cpp
)

target_include_directories(common PUBLIC include)
target_link_libraries(common PUBLIC spdlog miniz)

if (UNIX OR APPLE)
    set_property(TARGET common PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()