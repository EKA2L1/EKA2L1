add_library(common
    include/common/path.h
    include/common/cvt.h
    include/common/log.h
    include/common/types.h
    include/common/algorithm.h
    include/common/bytepair.h
    include/common/data_displayer.h
    src/path.cpp
    src/log.cpp
    src/cvt.cpp
    src/bytepair.cpp
    src/data_displayer.cpp)

target_include_directories(common PUBLIC include)
target_link_libraries(common PUBLIC spdlog)
