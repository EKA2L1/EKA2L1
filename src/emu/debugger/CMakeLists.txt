add_library(debugger
    include/debugger/debugger.h
    include/debugger/imgui_debugger.h
    include/debugger/logger.h
    include/debugger/renderer/renderer.h
    src/debugger.cpp
    src/imgui_debugger.cpp
    src/logger.cpp
    src/renderer/renderer.cpp
)

target_include_directories(debugger PUBLIC include)
target_link_libraries(debugger PRIVATE
    arm
    common 
    disasm
    epoc
    epockern
    epocloader
    manager
    nativefilediag
    imgui
    drivers
    stb
    yaml-cpp)
