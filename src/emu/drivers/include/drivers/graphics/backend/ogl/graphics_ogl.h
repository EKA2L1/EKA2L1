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
#include <drivers/graphics/backend/ogl/input_desc_ogl.h>
#include <drivers/graphics/context.h>

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
        GLint last_blend_src_rgb;
        GLint last_blend_src_alpha;
        GLint last_blend_dst_rgb;
        GLint last_blend_dst_alpha;
        GLint last_blend_equation_rgb;
        GLint last_blend_equation_alpha;
        GLint last_viewport[4];
        GLint last_scissor[4];
        GLboolean last_enable_blend;
        GLboolean last_enable_cull_face;
        GLboolean last_enable_depth_test;
        GLboolean last_enable_scissor_test;
    };

    enum ogl_driver_feature {
        OGL_FEATURE_SUPPORT_ETC2 = 1 << 0,
        OGL_FEATURE_SUPPORT_PVRTC = 1 << 1,
        OGL_MAX_FEATURE = 2
    };

    class ogl_graphics_driver : public shared_graphics_driver {
        std::unique_ptr<graphics::gl_context> context_;

        eka2l1::request_queue<command_list> list_queue;

        std::unique_ptr<ogl_shader_program> sprite_program;
        std::unique_ptr<ogl_shader_program> brush_program;
        std::unique_ptr<ogl_shader_program> mask_program;
        std::unique_ptr<ogl_shader_program> pen_program;

        GLuint sprite_vao;
        GLuint sprite_vbo;
        GLuint sprite_ibo;

        GLuint brush_vao;
        GLuint brush_vbo;
        GLuint pen_vao;
        GLuint pen_vbo;
        GLuint pen_ibo;

        GLint color_loc;
        GLint proj_loc;
        GLint model_loc;
        GLint flip_loc;

        GLint color_loc_brush;
        GLint proj_loc_brush;
        GLint model_loc_brush;

        GLint color_loc_mask;
        GLint proj_loc_mask;
        GLint model_loc_mask;
        GLint invert_loc_mask;
        GLint source_loc_mask;
        GLint mask_loc_mask;
        GLint flip_loc_mask;
        GLint flat_blend_loc_mask;

        GLint in_position_loc;
        GLint in_texcoord_loc;

        GLint in_position_loc_mask;
        GLint in_texcoord_loc_mask;

        GLint color_loc_pen;
        GLint proj_loc_pen;
        GLint model_loc_pen;
        GLint point_size_loc_pen;
        GLint pattern_bytes_loc_pen;
        GLint viewport_loc_pen;

        ogl_state backup;
        std::atomic_bool should_stop;
        std::atomic_bool surface_update_needed;

        input_descriptors_ogl *active_input_descriptors_;

        GLuint vbo_slots_[GL_BACKEND_MAX_VBO_SLOTS];
        GLuint index_buffer_current_;

        void *new_surface;
        bool is_gles;
        bool support_line_width_;

        float point_size;
        pen_style line_style;

        std::uint32_t feature_flags_;

        void do_init();
        void prepare_draw_lines_shared();

        void clear(command &cmd);
        void draw_bitmap(command &cmd);
        void draw_rectangle(command &cmd);
        void clip_rect(command &cmd);
        void draw_indexed(command &cmd);
        void draw_array(command &cmd);
        void set_viewport(command &cmd);
        void set_feature(command &cmd);
        void blend_formula(command &cmd);
        void set_stencil_action(command &cmd);
        void set_stencil_pass_condition(command &cmd);
        void set_stencil_mask(command &cmd);
        void set_depth_mask(command &cmd);
        void display(command &cmd);
        void set_point_size(command &cmd);
        void set_pen_style(command &cmd);
        void draw_line(command &cmd);
        void draw_polygon(command &cmd);
        void set_cull_face(command &cmd);
        void set_front_face_rule(command &cmd);
        void set_color_mask(command &cmd);
        void set_depth_func(command &cmd);
        void set_uniform(command &cmd);
        void set_texture_for_shader(command &cmd);
        void bind_vertex_buffers(command &cmd);
        void bind_index_buffer(command &cmd);
        void bind_input_descriptors(command &cmd);
        void set_line_width(command &cmd);
        void set_depth_bias(command &cmd);
        void set_depth_range(command &cmd);

        void save_gl_state();
        void load_gl_state();

    public:
        explicit ogl_graphics_driver(const window_system_info &info);
        ~ogl_graphics_driver() override {}

        void set_viewport(const eka2l1::rect &viewport) override;
        void submit_command_list(command_list &cmd_list) override;

        void run() override;
        void abort() override;
        void dispatch(command &cmd) override;
        void bind_swapchain_framebuf() override;
        void update_surface(void *new_surface) override;

        bool is_stricted() const override {
            return is_gles;
        }

        bool get_supported_feature(const std::uint32_t feature_mask) const {
            return feature_flags_ & feature_mask;
        }
    };
}
