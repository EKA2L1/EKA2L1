add_library(uvlooper STATIC
    src/uvlooper.cpp)
target_include_directories(uvlooper PUBLIC include)
target_link_libraries(uvlooper PRIVATE uv_a)