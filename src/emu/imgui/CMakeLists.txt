add_subdirectory(opengl)

add_executable(eka2l1_imgui EKA2L1.cpp
    internal.h internal.cpp
    window.h window.cpp
    imguirdr.h)

target_link_libraries(eka2l1_imgui imguirdr-gl imgui glfw GL X11 glad)
