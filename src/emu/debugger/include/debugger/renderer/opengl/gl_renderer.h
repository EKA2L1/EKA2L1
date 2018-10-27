#pragma once

#include <debugger/renderer/renderer.h>

#include <drivers/graphics/backend/ogl/shader_ogl.h>
#include <drivers/graphics/backend/ogl/texture_ogl.h>

namespace eka2l1 {
    using GLuint = unsigned int;
    
    class debugger_gl_renderer: public debugger_renderer {
        GLuint vbo_handle;
        GLuint vao_handle;
        GLuint elements_handle;
        GLuint attrib_loc_tex;
        GLuint attrib_loc_proj_matrix;
        GLuint attrib_loc_pos;
        GLuint attrib_loc_uv;
        GLuint attrib_loc_color;

        drivers::ogl_shader shader;
        drivers::ogl_texture font_texture;

    public:
        void init(drivers::graphics_driver_ptr driver, debugger_ptr &debugger) override;
        void draw(std::uint32_t width, std::uint32_t height
            , std::uint32_t fb_width, std::uint32_t fb_height) override;
        void deinit() override;
    };
}