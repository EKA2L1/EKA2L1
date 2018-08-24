add_library(common
    include/common/path.h
    include/common/cvt.h
    include/common/log.h
    include/common/types.h
    include/common/algorithm.h
    include/common/bytepair.h
    include/common/data_displayer.h
    include/common/flate.h
    include/common/resource.h
    include/common/buffer.h
    include/common/hash.h
    include/common/advstream.h
    src/algorithm.cpp
    src/path.cpp
    src/log.cpp
    src/cvt.cpp
    src/bytepair.cpp
    src/data_displayer.cpp
    src/flate.cpp
    src/types.cpp
    src/random.cpp
    src/hash.cpp)

target_include_directories(common PUBLIC include)
target_link_libraries(common PUBLIC spdlog miniz)
