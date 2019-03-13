/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <drivers/graphics/imgui_renderer.h>

#include <drivers/graphics/backend/ogl/shader_ogl.h>
#include <drivers/graphics/backend/ogl/texture_ogl.h>

using GLuint = unsigned int;

namespace eka2l1::drivers {
    class ogl_imgui_renderer : public imgui_renderer_base {
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
        void init() override;
        void render(ImDrawData *draw_data) override;
        void deinit() override;
    };
}
