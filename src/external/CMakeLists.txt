add_library(miniz miniz/miniz.h miniz/miniz.c)
target_include_directories(miniz PUBLIC miniz)

add_subdirectory(spdlog)

add_subdirectory(glfw)

add_library(imgui imgui/imgui.h imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_internal.h
    imgui/imconfig.h
    imgui/stb_rect_pack.h
    imgui/stb_textedit.h
    imgui/stb_truetype.h
)

target_include_directories(imgui PUBLIC imgui)

add_library(glad glad/include/glad/glad.h glad/src/glad.c)
target_include_directories(glad PUBLIC glad/include)
add_library(unicorn INTERFACE)

if(APPLE)
	target_include_directories(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/macos/include")
	target_link_libraries(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/macos/lib/libunicorn.a")
elseif(WIN32)
	target_include_directories(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/windows/include")
	target_link_libraries(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/windows/unicorn.lib")
elseif(UNIX)
        target_include_directories(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/unix/include")
        target_link_libraries(unicorn INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/unicorn/unix/lib/libunicorn.a")
endif()

