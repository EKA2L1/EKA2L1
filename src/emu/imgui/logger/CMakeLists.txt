add_library(imgui-logger logger.h logger.cpp)
target_link_libraries(imgui-logger PRIVATE imgui common)
target_include_directories(imgui-logger PUBLIC .)
