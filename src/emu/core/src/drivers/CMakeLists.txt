add_library(drivers
    ${CORE_INCLUDE_DIR}/drivers/emu_window.h
    ${CORE_INCLUDE_DIR}/drivers/screen_driver.h
    ${CORE_INCLUDE_DIR}/drivers/backend/emu_window_glfw.h
    ${CORE_INCLUDE_DIR}/drivers/backend/screen_driver_gl.h
    backend/emu_window_glfw.cpp
    backend/ogl/screen_driver_ogl.cpp)

target_include_directories(drivers PUBLIC ${CORE_INCLUDE_DIR} 
    ${FREETYPE_INCLUDE_DIR}
    ${FRSML_INCLUDE_DIR})

target_link_libraries(drivers PUBLIC common)
target_link_libraries(drivers PRIVATE glfw glad freetype frsml)