add_library(miniz miniz/miniz.h miniz/miniz.c)
target_include_directories(miniz PUBLIC miniz)

add_subdirectory(spdlog)
add_subdirectory(glfw)

add_library(imgui imgui/imgui.h imgui/imgui.cpp)
target_include_directories(imgui PUBLIC imgui)


