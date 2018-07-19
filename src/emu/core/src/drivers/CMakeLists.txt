add_library(drivers
    ${CORE_INCLUDE_DIR}/drivers/emu_window.h
    ${CORE_INCLUDE_DIR}/drivers/screen_driver.h
    ${CORE_INCLUDE_DIR}/drivers/backend/emu_window_glfw.h
    ${CORE_INCLUDE_DIR}/drivers/backend/ogl/screen_driver_ogl.h
    ${CORE_INCLUDE_DIR}/drivers/backend/ogl/instance_ogl.h
    ${CORE_INCLUDE_DIR}/drivers/backend/ogl/fb_ogl.h
    backend/emu_window_glfw.cpp
    backend/ogl/screen_driver_ogl.cpp
    backend/ogl/instance_ogl.cpp
    backend/ogl/shader_ogl.cpp
    emu_window.cpp
    screen_driver.cpp)

target_include_directories(drivers PUBLIC ${CORE_OUTER_INCLUDE_DIR} 
    ${FREETYPE_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR})

target_link_libraries(drivers PUBLIC common)
target_link_libraries(drivers PRIVATE glfw glad freetype)