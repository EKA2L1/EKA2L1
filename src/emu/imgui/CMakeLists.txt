add_subdirectory(opengl)
add_subdirectory(io)
add_subdirectory(logger)
add_subdirectory(installer)

add_executable(eka2l1_imgui EKA2L1.cpp
    EKA2L1.h
    internal.h internal.cpp
    window.h window.cpp
    imguirdr.h
    menu.h
    menu.cpp
    main.cpp
    memory_editor.h)

target_link_libraries(eka2l1_imgui PRIVATE imguirdr-gl imgui_io
    imgui common imgui-logger coreloader
        imgui_igs core imgui_sis_installer)
