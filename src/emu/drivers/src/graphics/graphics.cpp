#include <drivers/graphics/backend/ogl/graphics_ogl.h>
#include <drivers/graphics/graphics.h>

#include <glad/glad.h>

namespace eka2l1::drivers {
    bool init_graphics_library(graphic_api api) {
        switch (api) {
        case graphic_api::opengl: {
            gladLoadGL();
            return true;
        }

        default:
            break;
        }

        return false;
    }

    graphics_driver_ptr create_graphics_driver(const graphic_api api, const vec2 &screen_size) {
        switch (api) {
        case graphic_api::opengl: {
            return std::make_shared<ogl_graphics_driver>(screen_size);
        }

        default:
            break;
        }

        return nullptr;
    }
}
