/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <drivers/graphics/backend/graphics_driver_shared.h>
#include <drivers/graphics/backend/ogl/shader_ogl.h>
#include <drivers/graphics/backend/ogl/texture_ogl.h>

#include <common/queue.h>
#include <glad/glad.h>

#include <memory>

namespace eka2l1::drivers {
    class ogl_graphics_driver : public shared_graphics_driver {
        std::unique_ptr<ogl_shader> sprite_program;
        GLuint sprite_vao;
        GLuint sprite_vbo;
        GLuint sprite_ibo;

        GLint projection_loc;
        GLint model_loc;
        GLint color_loc;

        void do_init();
        void draw_bitmap(command_helper &helper);
        void set_invalidate(command_helper &helper);
        void invalidate_rect(command_helper &helper);

    public:
        explicit ogl_graphics_driver();
    };
}
