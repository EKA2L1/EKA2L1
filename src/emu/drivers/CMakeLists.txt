add_library(drivers 
    include/drivers/itc.h
    include/drivers/driver.h
    include/drivers/graphics/emu_window.h
    include/drivers/graphics/fb.h
    include/drivers/graphics/graphics.h
    include/drivers/graphics/shader.h
    include/drivers/graphics/texture.h
    include/drivers/graphics/backend/emu_window_glfw.h
    include/drivers/graphics/backend/ogl/fb_ogl.h
    include/drivers/graphics/backend/ogl/graphics_ogl.h
    include/drivers/graphics/backend/ogl/shader_ogl.h
    include/drivers/graphics/backend/ogl/texture_ogl.h
    src/driver.cpp
    src/itc.cpp
    src/graphics/emu_window.cpp
    src/graphics/graphics.cpp
    src/graphics/backend/emu_window_glfw.cpp
    src/graphics/backend/ogl/fb_ogl.cpp
    src/graphics/backend/ogl/graphics_ogl.cpp
    src/graphics/backend/ogl/imgui_impl_opengl3.cpp
    src/graphics/backend/ogl/imgui_impl_opengl3.h
    src/graphics/backend/ogl/texture_ogl.cpp
    src/graphics/backend/ogl/shader_ogl.cpp)

target_link_libraries(drivers PRIVATE common imgui glad glfw)
target_include_directories(drivers PUBLIC include)