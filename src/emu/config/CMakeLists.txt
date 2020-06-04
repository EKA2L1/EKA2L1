add_library(config
        include/config/config.h
        src/config.cpp)

target_include_directories(config PUBLIC include)
target_link_libraries(config PRIVATE
        common
        yaml-cpp)
