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
        src/elements/imgui_debugger_about.cpp
        src/elements/imgui_debugger_app_launcher.cpp
        src/elements/imgui_debugger_emu_screen.cpp
        src/elements/imgui_debugger_install_device.cpp
        src/elements/imgui_debugger_os_debug.cpp
        src/elements/imgui_debugger_package.cpp
        src/elements/imgui_debugger_prefs.cpp
        src/elements/imgui_debugger_user_utils.cpp
        src/renderer/common.cpp
        src/renderer/renderer.cpp
        src/renderer/spritesheet.cpp
        )

# Currently ImGui does not detect newline from the final
# formatted string but only in the type format string (first
# argument), so we have to use a variable for the format string.
# To prevent the build from failing due to this we disable
# format security errors.
if ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
        set_source_files_properties(
                src/elements/imgui_debugger_install_device.cpp
                PROPERTIES
                COMPILE_FLAGS
                "-Wno-format-security")
endif()

target_include_directories(debugger PUBLIC include)
target_link_libraries(debugger PRIVATE
        cpu
        common
        disasm
        drivers
        epoc
        epockern
        epocpkg
        epocservs
        imgui
        pugixml
        stb
        yaml-cpp)
