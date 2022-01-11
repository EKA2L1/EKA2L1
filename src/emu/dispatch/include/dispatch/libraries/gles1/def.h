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

#include <dispatch/libraries/egl/def.h>
#include <dispatch/libraries/gles1/consts.h>
#include <dispatch/def.h>

#include <common/container.h>
#include <common/vecx.h>

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

    enum gles1_object_type {
        GLES1_OBJECT_UNDEFINED,
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

        virtual gles1_object_type object_type() const {
            return GLES1_OBJECT_UNDEFINED;
        }
    };

    struct gles1_driver_texture : public gles1_driver_object {
    private:
        std::uint32_t internal_format_;
        std::uint32_t min_filter_;
        std::uint32_t mag_filter_;
        std::uint32_t wrap_s_;
        std::uint32_t wrap_t_;
        std::uint32_t mip_count_;

        bool auto_regen_mipmap_;

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

        void set_min_filter(const std::uint32_t min_filter) {
            min_filter_ = min_filter;
        }

        std::uint32_t get_min_filter() const {
            return min_filter_;
        }

        void set_mag_filter(const std::uint32_t mag_filter) {
            mag_filter_ = mag_filter;
        }

        std::uint32_t get_mag_filter() const {
            return mag_filter_;
        }

        void set_wrap_s(const std::uint32_t wrap_s) {
            wrap_s_ = wrap_s;
        }

        std::uint32_t get_wrap_s() const {
            return wrap_s_;
        }

        void set_wrap_t(const std::uint32_t wrap_t) {
            wrap_t_ = wrap_t;
        }

        std::uint32_t get_wrap_t() const {
            return wrap_t_;
        }

        void set_mip_count(const std::uint32_t count) {
            mip_count_ = count;
        }

        std::uint32_t get_mip_count() const {
            return mip_count_;
        }

        bool auto_regenerate_mipmap() const {
            return auto_regen_mipmap_;
        }

        void set_auto_regenerate_mipmap(const bool opt) {
            auto_regen_mipmap_ = opt;
        }

        eka2l1::vec2 size() const {
            return size_;
        }

        gles1_object_type object_type() const override {
            return GLES1_OBJECT_TEXTURE;
        }
    };

    struct gles1_driver_buffer : public gles1_driver_object {
    private:
        std::uint32_t data_size_;

    public:
        explicit gles1_driver_buffer(egl_context_es1 *ctx);

        void assign_data_size(const std::uint32_t data_size) {
            data_size_ = data_size;
        }

        const std::uint32_t data_size() const {
            return data_size_;
        }

        gles1_object_type object_type() const override {
            return GLES1_OBJECT_BUFFER;
        }
    };

    struct gles1_vertex_attrib {
        std::int32_t size_;
        std::uint32_t data_type_;
        std::int32_t stride_;
        std::uint32_t offset_;
        std::uint32_t buffer_obj_ = 0;

        drivers::handle in_house_buffer_ = 0;
    };

    struct gles_texture_env_info {
        enum environment_mode {
            ENV_MODE_ADD = 0,
            ENV_MODE_MODULATE = 1,
            ENV_MODE_DECAL = 2,
            ENV_MODE_BLEND = 3,
            ENV_MODE_REPLACE = 4,
            ENV_MODE_COMBINE = 5
        };

        enum source_combine_function {
            SOURCE_COMBINE_REPLACE = 0,
            SOURCE_COMBINE_MODULATE = 1,
            SOURCE_COMBINE_ADD = 2,
            SOURCE_COMBINE_ADD_SIGNED = 3,
            SOURCE_COMBINE_INTERPOLATE = 4,
            SOURCE_COMBINE_SUBTRACT = 5,
            SOURCE_COMBINE_DOT3_RGB = 6,
            SOURCE_COMBINE_DOT3_RGBA = 7
        };

        enum source_type {
            SOURCE_TYPE_CURRENT_TEXTURE = 0,
            SOURCE_TYPE_TEXTURE_STAGE_0 = 1,
            SOURCE_TYPE_TEXTURE_STAGE_1 = 2,
            SOURCE_TYPE_TEXTURE_STAGE_2 = 3,
            SOURCE_TYPE_CONSTANT = 4,
            SOURCE_TYPE_PRIM_COLOR = 5,
            SOURCE_TYPE_PREVIOUS = 6
        };

        enum source_operand {
            SOURCE_OPERAND_COLOR = 0,
            SOURCE_OPERAND_ONE_MINUS_COLOR = 1,
            SOURCE_OPERAND_ALPHA = 2,
            SOURCE_OPERAND_ONE_MINUS_ALPHA = 3
        };

        std::uint64_t env_mode_ : 3;
        std::uint64_t src0_rgb_ : 3;
        std::uint64_t src1_rgb_ : 3;
        std::uint64_t src2_rgb_ : 3;
        std::uint64_t src0_a_ : 3;
        std::uint64_t src1_a_ : 3;
        std::uint64_t src2_a_ : 3;
        std::uint64_t src0_rgb_op_ : 2;
        std::uint64_t src1_rgb_op_ : 2;
        std::uint64_t src2_rgb_op_ : 2;
        std::uint64_t src0_a_op_ : 2;
        std::uint64_t src1_a_op_ : 2;
        std::uint64_t src2_a_op_ : 2;
        std::uint64_t combine_rgb_func_ : 3;
        std::uint64_t combine_a_func_ : 3;
    };

    struct gles_texture_unit {
        std::size_t index_;

        std::stack<glm::mat4> texture_mat_stack_;
        std::uint32_t binded_texture_handle_;
        
        gles1_vertex_attrib coord_attrib_;
        gles_texture_env_info env_info_;

        std::uint8_t alpha_scale_;
        std::uint8_t rgb_scale_;

        float coord_uniforms_[4];
        float env_colors_[4];

        explicit gles_texture_unit();
    };

    struct gles1_light_info {
        float ambient_[4];
        float diffuse_[4];
        float specular_[4];
        float position_or_dir_[4];
        float spot_dir_[3];
        float spot_exponent_;
        float spot_cutoff_;
        float attenuatation_[3];

        float position_or_dir_transformed_[4];
        float spot_dir_transformed_[3];

        explicit gles1_light_info();
    };

    using gles1_driver_object_instance = std::unique_ptr<gles1_driver_object>;

    struct egl_context_es1 : public egl_context {
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
        drivers::blend_factor source_blend_factor_;
        drivers::blend_factor dest_blend_factor_;

        common::identity_container<gles1_driver_object_instance> objects_;

        std::stack<drivers::handle> texture_pools_;
        std::stack<drivers::handle> buffer_pools_;
        
        enum {
            FRAGMENT_STATE_ALPHA_TEST = 1 << 0,
            FRAGMENT_STATE_ALPHA_TEST_FUNC_POS = 1,
            FRAGMENT_STATE_ALPHA_FUNC_MASK = 0b1110,
            FRAGMENT_STATE_SHADE_MODEL_FLAT = 1 << 4,
            FRAGMENT_STATE_FOG_ENABLE = 1 << 5,
            FRAGMENT_STATE_FOG_MODE_POS = 6,
            FRAGMENT_STATE_FOG_MODE_MASK = 0b11000000,
            FRAGMENT_STATE_FOG_MODE_LINEAR = 0b00000000,
            FRAGMENT_STATE_FOG_MODE_EXP = 0b01000000,
            FRAGMENT_STATE_FOG_MODE_EXP2 = 0b10000000,
            FRAGMENT_STATE_LIGHTING_ENABLE = 1 << 8,
            FRAGMENT_STATE_LIGHT0_ON = 1 << 9,
            FRAGMENT_STATE_LIGHT1_ON = 1 << 10,
            FRAGMENT_STATE_LIGHT2_ON = 1 << 11,
            FRAGMENT_STATE_LIGHT3_ON = 1 << 12,
            FRAGMENT_STATE_LIGHT4_ON = 1 << 13,
            FRAGMENT_STATE_LIGHT5_ON = 1 << 14,
            FRAGMENT_STATE_LIGHT6_ON = 1 << 15,
            FRAGMENT_STATE_LIGHT7_ON = 1 << 16,
            FRAGMENT_STATE_LIGHT_TWO_SIDE = 1 << 17,
            FRAGMENT_STATE_LIGHT_RELATED_MASK = 0b111111111000000000,
            FRAGMENT_STATE_TEXTURE_ENABLE = 1 << 18,
            FRAGMENT_STATE_CLIP_PLANE_BIT_POS = 19,
            FRAGMENT_STATE_CLIP_PLANE0_ENABLE = 1 << 19,
            FRAGMENT_STATE_CLIP_PLANE1_ENABLE = 1 << 20,
            FRAGMENT_STATE_CLIP_PLANE2_ENABLE = 1 << 21,
            FRAGMENT_STATE_CLIP_PLANE3_ENABLE = 1 << 22,
            FRAGMENT_STATE_CLIP_PLANE4_ENABLE = 1 << 23,
            FRAGMENT_STATE_CLIP_PLANE5_ENABLE = 1 << 24,
            FRAGMENT_STATE_COLOR_MATERIAL_ENABLE = 1 << 25,

            VERTEX_STATE_CLIENT_VERTEX_ARRAY = 1 << 0,
            VERTEX_STATE_CLIENT_COLOR_ARRAY = 1 << 1,
            VERTEX_STATE_CLIENT_NORMAL_ARRAY = 1 << 2,
            VERTEX_STATE_CLIENT_TEXCOORD_ARRAY = 1 << 3,
            VERTEX_STATE_CLIENT_ARRAY_MASK = 0b1111,
            VERTEX_STATE_NORMAL_ENABLE_RESCALE = 1 << 4,
            VERTEX_STATE_NORMAL_ENABLE_NORMALIZE = 1 << 5,

            NON_SHADER_STATE_BLEND_ENABLE = 1 << 0,
            NON_SHADER_STATE_COLOR_LOGIC_OP_ENABLE = 1 << 1,
            NON_SHADER_STATE_CULL_FACE_ENABLE = 1 << 2,
            NON_SHADER_STATE_DEPTH_TEST_ENABLE = 1 << 4,
            NON_SHADER_STATE_STENCIL_TEST_ENABLE = 1 << 5,
            NON_SHADER_STATE_LINE_SMOOTH = 1 << 6,
            NON_SHADER_STATE_DITHER = 1 << 7,
            NON_SHADER_STATE_SCISSOR_ENABLE = 1 << 8,
            NON_SHADER_STATE_POLYGON_OFFSET_FILL = 1 << 9,
            NON_SHADER_STATE_MULTISAMPLE = 1 << 10,
            NON_SHADER_STATE_SAMPLE_ALPHA_TO_COVERAGE = 1 << 11,
            NON_SHADER_STATE_SAMPLE_ALPHA_TO_ONE = 1 << 12,
            NON_SHADER_STATE_SAMPLE_COVERAGE = 1 << 13,

            STATE_CHANGED_CULL_FACE = 1 << 0,
            STATE_CHANGED_SCISSOR_RECT = 1 << 1,
            STATE_CHANGED_VIEWPORT_RECT = 1 << 2,
            STATE_CHANGED_FRONT_FACE_RULE = 1 << 3,
            STATE_CHANGED_COLOR_MASK = 1 << 4,
            STATE_CHANGED_DEPTH_BIAS = 1 << 5,
            STATE_CHANGED_STENCIL_MASK = 1 << 6,
            STATE_CHANGED_BLEND_FACTOR = 1 << 7,
            STATE_CHANGED_LINE_WIDTH = 1 << 8,
            STATE_CHANGED_DEPTH_MASK = 1 << 9,
            STATE_CHANGED_DEPTH_PASS_COND = 1 << 10,
            STATE_CHANGED_DEPTH_RANGE = 1 << 11
        };

        gles1_vertex_attrib vertex_attrib_;
        gles1_vertex_attrib color_attrib_;
        gles1_vertex_attrib normal_attrib_;
        drivers::handle input_desc_;

        bool attrib_changed_;

        float color_uniforms_[4];
        float normal_uniforms_[3];

        // Material
        float material_ambient_[4];
        float material_diffuse_[4];
        float material_specular_[4];
        float material_emission_[4];
        float material_shininess_;

        // Fog
        float fog_density_;
        float fog_start_;
        float fog_end_;
        float fog_color_[4];

        // Lights
        gles1_light_info lights_[8];
        float global_ambient_[4];

        // Viewport
        eka2l1::rect viewport_bl_;
        eka2l1::rect scissor_bl_;

        // Clip planes
        float clip_planes_[GLES1_EMU_MAX_CLIP_PLANE][4];
        float clip_planes_transformed_[GLES1_EMU_MAX_CLIP_PLANE][4];

        // Lines
        float line_width_;

        std::uint64_t vertex_statuses_;
        std::uint64_t fragment_statuses_;
        std::uint64_t non_shader_statuses_;
        std::uint64_t state_change_tracker_;

        std::uint8_t color_mask_;
        std::uint32_t stencil_mask_;
        std::uint32_t depth_mask_;
        std::uint32_t depth_func_;

        float alpha_test_ref_;

        drivers::handle index_buffer_temp_;
        std::size_t index_buffer_temp_size_;

        float polygon_offset_factor_;
        float polygon_offset_units_;

        std::uint32_t pack_alignment_;
        std::uint32_t unpack_alignment_;

        float depth_range_min_;
        float depth_range_max_;

        explicit egl_context_es1();

        glm::mat4 &active_matrix();

        gles1_driver_texture *binded_texture();
        gles1_driver_buffer *binded_buffer(const bool is_array_buffer);
        drivers::handle binded_texture_driver_handle();

        void init_context_state() override;
        void free(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder) override;
        void return_handle_to_pool(const gles1_object_type type, const drivers::handle h);
        void on_surface_changed(egl_surface *prev_read, egl_surface *prev_draw) override;
        void flush_stage_changes();

        egl_context_type context_type() const override {
            return EGL_GLES1_CONTEXT;
        }
    };

    egl_context_es1 *get_es1_active_context(system *sys);
}