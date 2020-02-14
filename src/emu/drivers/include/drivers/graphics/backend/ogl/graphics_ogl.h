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
#include <queue>

namespace eka2l1::drivers {
    struct ogl_state {
        GLint last_program;
        GLint last_texture;
        GLint last_active_texture;
        GLint last_array_buffer;
        GLint last_element_array_buffer;
        GLint last_vertex_array;
        GLint last_blend_src;
        GLint last_blend_dst;
        GLint last_blend_equation_rgb;
        GLint last_blend_equation_alpha;
        GLint last_viewport[4];
        GLint last_scissor[4];
        GLboolean last_enable_blend;
        GLboolean last_enable_cull_face;
        GLboolean last_enable_depth_test;
        GLboolean last_enable_scissor_test;
    };

    class ogl_graphics_driver : public shared_graphics_driver {
        eka2l1::request_queue<server_graphics_command_list> list_queue;
        std::unique_ptr<ogl_shader> sprite_program;
        std::unique_ptr<ogl_shader> fill_program;
        std::unique_ptr<ogl_shader> mask_program;

        GLuint sprite_vao;
        GLuint sprite_vbo;
        GLuint sprite_ibo;

        GLuint fill_vao;
        GLuint fill_vbo;

        GLint color_loc;
        GLint proj_loc;
        GLint model_loc;
        
        GLint color_loc_fill;
        GLint proj_loc_fill;
        GLint model_loc_fill;

        GLint color_loc_mask;
        GLint proj_loc_mask;
        GLint model_loc_mask;
        GLint invert_loc_mask;
        GLint source_loc_mask;
        GLint mask_loc_mask;
        GLint alphablending_loc_mask;

        ogl_state backup;
        std::atomic_bool should_stop;

        void do_init();

        void clear(command_helper &helper);
        void draw_bitmap(command_helper &helper);
        void draw_rectangle(command_helper &helper);
        void set_invalidate(command_helper &helper);
        void invalidate_rect(command_helper &helper);
        void draw_indexed(command_helper &helper);
        void set_viewport(command_helper &helper);
        void set_depth(command_helper &helper);
        void set_blend(command_helper &helper);
        void set_cull(command_helper &helper);
        void blend_formula(command_helper &helper);
        void display(command_helper &helper);

        void save_gl_state();
        void load_gl_state();

    public:
        explicit ogl_graphics_driver();
        ~ogl_graphics_driver() override {}

        void set_viewport(const eka2l1::rect &viewport) override;
        std::unique_ptr<graphics_command_list> new_command_list() override;
        void submit_command_list(graphics_command_list &command_list) override;
        std::unique_ptr<graphics_command_list_builder> new_command_builder(graphics_command_list *list) override;

        void run() override;
        void abort() override;
        void dispatch(command *cmd) override;
        void bind_swapchain_framebuf() override;
    };
}
