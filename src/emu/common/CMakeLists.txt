add_library(common
    include/common/path.h
    include/common/ptr.h
    src/path.cpp)

target_include_directories(common PUBLIC include)
