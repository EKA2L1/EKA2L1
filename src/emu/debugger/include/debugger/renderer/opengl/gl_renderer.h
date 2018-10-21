#pragma once

#include <debugger/renderer/renderer.h>

namespace eka2l1 {
    using GLuint = unsigned int;
    
    class debugger_gl_renderer: public debugger_renderer {
        GLuint font_texture;
        GLuint shader_handle;
        GLuint vert_handle;
        GLuint frag_handle;
        GLuint vbo_handle;
        GLuint vao_handle;
        GLuint elements_handle;
        GLuint attrib_loc_tex;
        GLuint attrib_loc_proj_matrix;
        GLuint attrib_loc_pos;
        GLuint attrib_loc_uv;
        GLuint attrib_loc_color;

    public:
        void init(debugger_ptr &debugger) override;
        void draw(std::uint32_t width, std::uint32_t height
            , std::uint32_t fb_width, std::uint32_t fb_height) override;
        void deinit() override;
    };
}