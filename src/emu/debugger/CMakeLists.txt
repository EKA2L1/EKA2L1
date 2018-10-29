add_library(debugger
    include/debugger/debugger.h
    include/debugger/imgui_debugger.h
    include/debugger/logger.h
    include/debugger/renderer/renderer.h
    include/debugger/renderer/opengl/gl_renderer.h
    src/debugger.cpp
    src/imgui_debugger.cpp
    src/logger.cpp
    src/renderer/renderer.cpp
    src/renderer/opengl/gl_renderer.cpp
)

target_include_directories(debugger PUBLIC include)
target_link_libraries(debugger PRIVATE common core imgui glad coreloader drivers)