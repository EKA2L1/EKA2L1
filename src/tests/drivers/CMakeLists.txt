add_executable(ekadrivertests
    graphics/ogl/spritebatch_ogl.cpp
    graphics/ogl/spritebatch_ogl.h
    graphics/scene.h
    graphics/spritebatch.cpp
    graphics/spritebatch.h
    graphics/texture.cpp
    main.cpp)

target_link_libraries(ekadrivertests
    PRIVATE common glad glfw glm drivers)

set(DRIVERS_ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/epoc/drivers/assets/")
add_test_assets(ekadrivertests ${DRIVERS_ASSETS_PATH} ".")
