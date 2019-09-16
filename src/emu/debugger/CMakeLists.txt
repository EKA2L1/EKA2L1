add_library(debugger
    include/debugger/debugger.h
    include/debugger/imgui_debugger.h
    include/debugger/logger.h
    include/debugger/renderer/common.h
    include/debugger/renderer/renderer.h
    include/debugger/renderer/spritesheet.h
    src/debugger.cpp
    src/imgui_debugger.cpp
    src/logger.cpp
    src/renderer/common.cpp
    src/renderer/renderer.cpp
    src/renderer/spritesheet.cpp
)

target_include_directories(debugger PUBLIC include)
target_link_libraries(debugger PRIVATE
    arm
    common 
    disasm
    drivers
    epoc
    epockern
    epocloader
    manager
    nativefilediag
    imgui
    pugixml
    stb
    yaml-cpp)
