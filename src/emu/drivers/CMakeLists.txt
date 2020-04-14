if (BUILD_WITH_VULKAN)
    set (DRIVERS_VULKAN_SRC  
        include/drivers/graphics/backend/vulkan/graphics_vulkan.h
        src/graphics/backend/vulkan/graphics_vulkan.cpp)
endif()

add_library(drivers 
    include/drivers/itc.h
    include/drivers/driver.h
    include/drivers/audio/audio.h
    include/drivers/audio/dsp.h
    include/drivers/audio/player.h
    include/drivers/audio/stream.h
    include/drivers/audio/backend/cubeb/audio_cubeb.h
    include/drivers/audio/backend/cubeb/stream_cubeb.h
    include/drivers/audio/backend/ffmpeg/dsp_ffmpeg.h
    include/drivers/audio/backend/ffmpeg/player_ffmpeg.h
    include/drivers/audio/backend/wmf/player_wmf.h
    include/drivers/audio/backend/dsp_shared.h
    include/drivers/audio/backend/player_shared.h
    include/drivers/graphics/buffer.h
    include/drivers/graphics/emu_window.h
    include/drivers/graphics/fb.h
    include/drivers/graphics/graphics.h
    include/drivers/graphics/imgui_renderer.h
    include/drivers/graphics/shader.h
    include/drivers/graphics/texture.h
    include/drivers/graphics/backend/emu_window_glfw.h
    include/drivers/graphics/backend/graphics_driver_shared.h
    include/drivers/graphics/backend/ogl/buffer_ogl.h
    include/drivers/graphics/backend/ogl/common_ogl.h
    include/drivers/graphics/backend/ogl/fb_ogl.h
    include/drivers/graphics/backend/ogl/graphics_ogl.h
    include/drivers/graphics/backend/ogl/shader_ogl.h
    include/drivers/graphics/backend/ogl/texture_ogl.h
    src/driver.cpp
    src/itc.cpp
    src/audio/audio.cpp
    src/audio/dsp.cpp
    src/audio/player.cpp
    src/audio/backend/cubeb/audio_cubeb.cpp
    src/audio/backend/cubeb/stream_cubeb.cpp
    src/audio/backend/ffmpeg/dsp_ffmpeg.cpp
    src/audio/backend/ffmpeg/player_ffmpeg.cpp
    src/audio/backend/wmf/player_wmf.cpp
    src/audio/backend/dsp_shared.cpp
    src/audio/backend/player_shared.cpp
    src/graphics/buffer.cpp
    src/graphics/fb.cpp
    src/graphics/emu_window.cpp
    src/graphics/imgui_renderer.cpp
    src/graphics/graphics.cpp
    src/graphics/shader.cpp
    src/graphics/texture.cpp
    src/graphics/backend/emu_window_glfw.cpp
    src/graphics/backend/graphics_driver_shared.cpp
    src/graphics/backend/ogl/buffer_ogl.cpp
    src/graphics/backend/ogl/common_ogl.cpp
    src/graphics/backend/ogl/fb_ogl.cpp
    src/graphics/backend/ogl/graphics_ogl.cpp
    src/graphics/backend/ogl/texture_ogl.cpp
    src/graphics/backend/ogl/shader_ogl.cpp
    ${DRIVERS_VULKAN_SRC})

target_link_libraries(drivers PRIVATE common cubeb imgui ffmpeg glad glm glfw)
target_include_directories(drivers PUBLIC include)

if (BUILD_WITH_VULKAN)
    target_link_libraries(drivers PRIVATE Vulkan)
endif()
