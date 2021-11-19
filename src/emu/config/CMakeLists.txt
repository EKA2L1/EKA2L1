add_library(config
        include/config/app_settings.h
        include/config/config.h
        include/config/panic_blacklist.h
        src/app_settings.cpp
        src/config.cpp
        src/panic_blacklist.cpp)

target_include_directories(config PUBLIC include)
target_link_libraries(config PRIVATE
        common
        yaml-cpp)
