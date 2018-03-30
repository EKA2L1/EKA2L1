add_library(common
    include/common/path.h
    include/common/ptr.h
    include/common/log.h
    src/path.cpp
    src/log.cpp)

target_include_directories(common PUBLIC include)
target_link_libraries(common PUBLIC spdlog)
