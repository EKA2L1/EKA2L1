add_library(debugger
    include/debugger/debugger.h
    include/debugger/renderer/renderer.h
    include/debugger/renderer/opengl/gl_renderer.h
    src/debugger.cpp
    src/renderer/renderer.cpp
    src/renderer/opengl/gl_renderer.cpp
)

target_include_directories(debugger PUBLIC include)
target_link_libraries(debugger PRIVATE core imgui glad)