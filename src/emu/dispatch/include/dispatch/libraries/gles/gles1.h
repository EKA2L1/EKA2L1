/*
 * Copyright (c) 2021 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <dispatch/libraries/gles/def.h>
#include <dispatch/def.h>

#include <common/container.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <memory>
#include <optional>
#include <stack>
#include <tuple>

namespace eka2l1 {
    class system;
}

namespace eka2l1::dispatch {
    #define FIXED_32_TO_FLOAT(x) (float)(x) / 65536.0f

    enum {
        GLES1_EMU_MAX_TEXTURE_SIZE = 4096,
        GLES1_EMU_MAX_TEXTURE_MIP_LEVEL = 10,
        GLES1_EMU_MAX_TEXTURE_COUNT = 16
    };

    enum gles1_object_type {
        GLES1_OBJECT_TEXTURE,
        GLES1_OBJECT_BUFFER
    };

    struct egl_context_es1;

    struct gles1_driver_object {
    protected:
        egl_context_es1 *context_;
        drivers::handle driver_handle_;

    public:
        explicit gles1_driver_object(egl_context_es1 *ctx);
        virtual ~gles1_driver_object();

        void assign_handle(const drivers::handle h) {
            driver_handle_ = h;
        }

        drivers::handle handle_value() const {
            return driver_handle_;
        }

        virtual gles1_object_type object_type() const = 0;
    };

    struct gles1_driver_texture : public gles1_driver_object {
    private:
        std::uint32_t internal_format_;
        eka2l1::vec2 size_;

    public:
        explicit gles1_driver_texture(egl_context_es1 *ctx);

        void set_internal_format(const std::uint32_t format) {
            internal_format_ = format;
        }

        std::uint32_t internal_format() const {
            return internal_format_;
        }

        void set_size(const eka2l1::vec2 size) {
            size_ = size;
        }

        eka2l1::vec2 size() const {
            return size_;
        }

        gles1_object_type object_type() const override {
            return GLES1_OBJECT_TEXTURE;
        }
    };

    struct gles1_driver_buffer : public gles1_driver_object {
    public:
        explicit gles1_driver_buffer(egl_context_es1 *ctx);

        gles1_object_type object_type() const override {
            return GLES1_OBJECT_BUFFER;
        }
    };

    struct gles1_vertex_attribs {
        std::int32_t size_;
        std::uint32_t data_type_;
        std::int32_t stride_;
        std::uint32_t offset_;

        bool is_active_ { false };
    };

    struct gles_texture_unit {
        std::size_t index_;

        std::stack<glm::mat4> texture_mat_stack_;
        std::uint32_t binded_texture_handle_;
        
        gles1_vertex_attribs coord_attrib_;
        float coord_uniforms_[4];

        explicit gles_texture_unit();
    };

    using gles1_driver_object_instance = std::unique_ptr<gles1_driver_object>;

    struct egl_context_es1 : public egl_context {
        std::unique_ptr<drivers::graphics_command_list> command_list_;
        std::unique_ptr<drivers::server_graphics_command_list_builder> command_builder_;

        float clear_color_[4];
        float clear_depth_;
        std::int32_t clear_stencil_;

        std::stack<glm::mat4> model_view_mat_stack_;
        std::stack<glm::mat4> proj_mat_stack_;

        gles_texture_unit texture_units_[GLES1_EMU_MAX_TEXTURE_COUNT];

        std::uint32_t active_texture_unit_;
        std::uint32_t active_client_texture_unit_;
        std::uint32_t active_mat_stack_;
        std::uint32_t binded_array_buffer_handle_;
        std::uint32_t binded_element_array_buffer_handle_;

        drivers::rendering_face active_cull_face_;
        drivers::rendering_face_determine_rule active_front_face_rule_;

        common::identity_container<gles1_driver_object_instance> objects_;

        std::stack<drivers::handle> texture_pools_;
        std::stack<drivers::handle> buffer_pools_;
        
        enum {
            STATE_ALPHA_TEST = 1 << 0,
            STATE_CLIENT_VERTEX_ARRAY = 1 << 1,
            STATE_CLIENT_COLOR_ARRAY = 1 << 2,
            STATE_CLIENT_NORMAL_ARRAY = 1 << 3,
            STATE_CLIENT_TEXCOORD_ARRAY = 1 << 4
        };

        gles1_vertex_attribs vertex_attrib_;
        gles1_vertex_attribs color_attrib_;
        gles1_vertex_attribs normal_attrib_;

        float color_uniforms_[4];
        float normal_uniforms_[3];

        std::uint64_t state_statuses_;

        float alpha_test_ref_;
        std::uint32_t alpha_test_func_;
        std::uint32_t shade_model_;

        explicit egl_context_es1();
        glm::mat4 &active_matrix();

        gles1_driver_texture *binded_texture();
        gles1_driver_buffer *binded_buffer(const bool is_array_buffer);
        drivers::handle binded_texture_driver_handle();

        void free(drivers::graphics_driver *driver, drivers::graphics_command_list_builder &builder) override;
        void return_handle_to_pool(const gles1_object_type type, const drivers::handle h);

        egl_context_type context_type() const override {
            return EGL_GLES1_CONTEXT;
        }
    };

    egl_context_es1 *get_es1_active_context(system *sys);

    BRIDGE_FUNC_DISPATCHER(void, gl_clear_color_emu, float red, float green, float blue, float alpha);
    BRIDGE_FUNC_DISPATCHER(void, gl_clear_colorx_emu, std::int32_t red, std::int32_t green, std::int32_t blue, std::int32_t alpha);
    BRIDGE_FUNC_DISPATCHER(void, gl_clear_depthf_emu, float depth);
    BRIDGE_FUNC_DISPATCHER(void, gl_clear_depthx_emu, std::int32_t depth);
    BRIDGE_FUNC_DISPATCHER(void, gl_clear_stencil, std::int32_t s);
    BRIDGE_FUNC_DISPATCHER(void, gl_clear_emu, std::uint32_t bits);
    BRIDGE_FUNC_DISPATCHER(void, gl_matrix_mode_emu, std::uint32_t mode);
    BRIDGE_FUNC_DISPATCHER(void, gl_push_matrix_emu);
    BRIDGE_FUNC_DISPATCHER(void, gl_pop_matrix_emu);
    BRIDGE_FUNC_DISPATCHER(void, gl_load_identity_emu, float *mat);
    BRIDGE_FUNC_DISPATCHER(void, gl_load_matrixf_emu, std::uint32_t *mat);
    BRIDGE_FUNC_DISPATCHER(std::uint32_t, gl_get_error_emu);
    BRIDGE_FUNC_DISPATCHER(void, gl_orthof_emu, float left, float right, float bottom, float top, float near, float far);
    BRIDGE_FUNC_DISPATCHER(void, gl_orthox_emu, std::uint32_t left, std::uint32_t right, std::uint32_t bottom, std::uint32_t top, std::uint32_t near, std::uint32_t far);
    BRIDGE_FUNC_DISPATCHER(void, gl_mult_matrixf_emu, float *m);
    BRIDGE_FUNC_DISPATCHER(void, gl_mult_matrixx_emu, std::uint32_t *m);
    BRIDGE_FUNC_DISPATCHER(void, gl_scalef_emu, float x, float y, float z);
    BRIDGE_FUNC_DISPATCHER(void, gl_scalex_emu, std::uint32_t x, std::uint32_t y, std::uint32_t z);
    BRIDGE_FUNC_DISPATCHER(void, gl_translatef_emu, float x, float y, float z);
    BRIDGE_FUNC_DISPATCHER(void, gl_translatex_emu, std::uint32_t x, std::uint32_t y, std::uint32_t z);
    BRIDGE_FUNC_DISPATCHER(void, gl_rotatef_emu, float angles, float x, float y, float z);
    BRIDGE_FUNC_DISPATCHER(void, gl_rotatex_emu, std::uint32_t angles, std::uint32_t x, std::uint32_t y, std::uint32_t z);
    BRIDGE_FUNC_DISPATCHER(void, gl_frustumf_emu, float left, float right, float bottom, float top, float near, float far);
    BRIDGE_FUNC_DISPATCHER(void, gl_frustumx_emu, std::uint32_t left, std::uint32_t right, std::uint32_t bottom, std::uint32_t top, std::uint32_t near, std::uint32_t far);
    BRIDGE_FUNC_DISPATCHER(void, gl_cull_face_emu, std::uint32_t mode);
    BRIDGE_FUNC_DISPATCHER(void, gl_scissor_emu, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height);
    BRIDGE_FUNC_DISPATCHER(void, gl_front_face_emu, std::uint32_t mode);
    BRIDGE_FUNC_DISPATCHER(void, gl_front_face_emu, std::uint32_t mode);
    BRIDGE_FUNC_DISPATCHER(bool, gl_is_texture_emu, std::uint32_t name);
    BRIDGE_FUNC_DISPATCHER(bool, gl_is_buffer_emu, std::uint32_t name);
    BRIDGE_FUNC_DISPATCHER(void, gl_gen_textures_emu, std::int32_t n, std::uint32_t *texs);
    BRIDGE_FUNC_DISPATCHER(void, gl_gen_buffers_emu, std::int32_t n, std::uint32_t *buffers);
    BRIDGE_FUNC_DISPATCHER(void, gl_delete_textures_emu, std::int32_t n, std::uint32_t *texs);
    BRIDGE_FUNC_DISPATCHER(void, gl_bind_texture_emu, std::uint32_t target, std::uint32_t name);
    BRIDGE_FUNC_DISPATCHER(void, gl_bind_buffer_emu, std::uint32_t target, std::uint32_t name);
    BRIDGE_FUNC_DISPATCHER(void, gl_tex_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t internal_format,
        std::int32_t width, std::int32_t height, std::int32_t border, std::uint32_t format, std::uint32_t data_type,
        void *data_pixels);
    BRIDGE_FUNC_DISPATCHER(void, gl_tex_sub_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t xoffset,
        std::int32_t yoffset, std::int32_t width, std::int32_t height, std::uint32_t format, std::uint32_t data_type,
        void *data_pixels);
    BRIDGE_FUNC_DISPATCHER(void, gl_alpha_func_emu, std::uint32_t func, float ref);
    BRIDGE_FUNC_DISPATCHER(void, gl_alpha_func_x_emu, std::uint32_t func, std::uint32_t ref);
    BRIDGE_FUNC_DISPATCHER(void, gl_normal_3f_emu, float nx, float ny, float nz);
    BRIDGE_FUNC_DISPATCHER(void, gl_normal_3x_emu, std::uint32_t nx, std::uint32_t ny, std::uint32_t nz);
    BRIDGE_FUNC_DISPATCHER(void, gl_color_4f_emu, float red, float green, float blue, float alpha);
    BRIDGE_FUNC_DISPATCHER(void, gl_color_4x_emu, std::uint32_t red, std::uint32_t green, std::uint32_t blue, std::uint32_t alpha);
    BRIDGE_FUNC_DISPATCHER(void, gl_color_4ub_emu, std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha);
    BRIDGE_FUNC_DISPATCHER(void, gl_shade_model_emu, std::uint32_t model);
    BRIDGE_FUNC_DISPATCHER(void, gl_active_texture_emu, std::uint32_t unit);
    BRIDGE_FUNC_DISPATCHER(void, gl_client_active_texture_emu, std::uint32_t unit);
    BRIDGE_FUNC_DISPATCHER(void, gl_multi_tex_coord_4f_emu, std::uint32_t unit, float s, float t, float r, float q);
    BRIDGE_FUNC_DISPATCHER(void, gl_multi_tex_coord_4x_emu, std::uint32_t unit, std::uint32_t s, std::uint32_t t, std::uint32_t r, std::uint32_t q);
    BRIDGE_FUNC_DISPATCHER(void, gl_enable_client_state_emu, std::uint32_t state);
    BRIDGE_FUNC_DISPATCHER(void, gl_disable_client_state_emu, std::uint32_t state);
}