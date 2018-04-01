add_library(manager
    include/manager/package_manager.h
    include/manager/manager.h
    src/package_manager.cpp
    src/manager.cpp)

target_include_directories(manager PUBLIC include)
target_link_libraries(manager PRIVATE common loader)
