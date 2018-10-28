add_library(debugger
    include/debugger/debugger.h
    include/debugger/logger.h
    include/debugger/renderer/renderer.h
    include/debugger/renderer/opengl/gl_renderer.h
    include/debugger/analysis/analysis.h 
    include/debugger/analysis/subroutine.h
    src/debugger.cpp
    src/logger.cpp
    src/renderer/renderer.cpp
    src/renderer/opengl/gl_renderer.cpp
    src/analysis/analysis.cpp
)

target_include_directories(debugger PUBLIC include)
target_link_libraries(debugger PRIVATE common core imgui glad coreloader drivers)