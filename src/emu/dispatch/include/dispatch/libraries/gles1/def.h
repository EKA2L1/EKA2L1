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
#include <dispatch/libraries/gles_shared/def.h>
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
    struct egl_context_es1;

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

    struct gles1_texture_unit {
        std::size_t index_;

        std::stack<glm::mat4> texture_mat_stack_;
        std::uint32_t binded_texture_handle_;
        
        gles_vertex_attrib coord_attrib_;
        gles_texture_env_info env_info_;

        std::uint8_t alpha_scale_;
        std::uint8_t rgb_scale_;

        float coord_uniforms_[4];
        float env_colors_[4];

        bool texturing_enabled_;

        explicit gles1_texture_unit();
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

    struct egl_context_es1 : public egl_context_es_shared {
    private:
        void prepare_vertex_buffer_and_descriptors(drivers::graphics_driver *drv, kernel::process *crr_process, const std::int32_t first_index, const std::uint32_t vcount, const std::uint32_t active_texs);
        bool prepare_shader_program_for_draw(dispatch::egl_controller &controller, const std::uint32_t active_texs);

    public:
        std::stack<glm::mat4> model_view_mat_stack_;
        std::stack<glm::mat4> proj_mat_stack_;
        std::array<glm::mat4, GLES1_EMU_MAX_PALETTE_MATRICES> palette_mats_; 

        gles1_texture_unit texture_units_[GLES1_EMU_MAX_TEXTURE_COUNT];
        std::uint32_t active_client_texture_unit_;
        std::uint32_t active_mat_stack_;
        std::uint32_t current_palette_mat_index_;

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
            FRAGMENT_STATE_CLIP_PLANE_BIT_POS = 18,
            FRAGMENT_STATE_CLIP_PLANE0_ENABLE = 1 << 18,
            FRAGMENT_STATE_CLIP_PLANE1_ENABLE = 1 << 19,
            FRAGMENT_STATE_CLIP_PLANE2_ENABLE = 1 << 20,
            FRAGMENT_STATE_CLIP_PLANE3_ENABLE = 1 << 21,
            FRAGMENT_STATE_CLIP_PLANE4_ENABLE = 1 << 22,
            FRAGMENT_STATE_CLIP_PLANE5_ENABLE = 1 << 23,
            FRAGMENT_STATE_COLOR_MATERIAL_ENABLE = 1 << 24,
            FRAGMENT_STATE_REVERSED_BITS_POS = 54,

            VERTEX_STATE_CLIENT_VERTEX_ARRAY = 1 << 0,
            VERTEX_STATE_CLIENT_COLOR_ARRAY = 1 << 1,
            VERTEX_STATE_CLIENT_NORMAL_ARRAY = 1 << 2,
            VERTEX_STATE_CLIENT_ARRAY_MASK = 0b1111,
            VERTEX_STATE_NORMAL_ENABLE_RESCALE = 1 << 4,
            VERTEX_STATE_NORMAL_ENABLE_NORMALIZE = 1 << 5,
            VERTEX_STATE_CLIENT_TEXCOORD_ARRAY_POS = 6,
            VERTEX_STATE_CLIENT_TEXCOORD0_ARRAY = 1 << 6,
            VERTEX_STATE_CLIENT_TEXCOORD1_ARRAY = 1 << 7,
            VERTEX_STATE_CLIENT_TEXCOORD2_ARRAY = 1 << 8,
            VERTEX_STATE_CLIENT_TEXCOORD3_ARRAY = 1 << 9,
            VERTEX_STATE_CLIENT_TEXCOORD4_ARRAY = 1 << 10,
            VERTEX_STATE_CLIENT_TEXCOORD5_ARRAY = 1 << 11,
            VERTEX_STATE_CLIENT_TEXCOORD6_ARRAY = 1 << 12,
            VERTEX_STATE_CLIENT_TEXCOORD7_ARRAY = 1 << 13,
            VERTEX_STATE_CLIENT_MATRIX_INDEX_ARRAY = 1 << 14,
            VERTEX_STATE_CLIENT_WEIGHT_ARRAY = 1 << 15,
            VERTEX_STATE_SKINNING_ENABLE = 1 << 16,
            VERTEX_STATE_SKIN_WEIGHTS_PER_VERTEX_BITS_POS = 17,
            VERTEX_STATE_SKIN_WEIGHTS_PER_VERTEX_MASK = 0b11 << 17,
            VERTEX_STATE_REVERSED_BITS_POS = 54
        };

        gles_vertex_attrib vertex_attrib_;
        gles_vertex_attrib color_attrib_;
        gles_vertex_attrib normal_attrib_;
        gles_vertex_attrib matrix_index_attrib_;
        gles_vertex_attrib weight_attrib_;
        drivers::handle input_desc_;

        std::int32_t previous_first_index_;
        std::uint32_t skin_weights_per_ver;

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

        // Clip planes
        float clip_planes_[GLES1_EMU_MAX_CLIP_PLANE][4];
        float clip_planes_transformed_[GLES1_EMU_MAX_CLIP_PLANE][4];

        std::uint64_t vertex_statuses_;
        std::uint64_t fragment_statuses_;
        float alpha_test_ref_;

        explicit egl_context_es1();
        glm::mat4 &active_matrix();

        gles_driver_texture *binded_texture() override;
        egl_context_type context_type() const override {
            return EGL_GLES1_CONTEXT;
        }

        bool prepare_for_draw(drivers::graphics_driver *driver, egl_controller &controller, kernel::process *crr_process,
            const std::int32_t first_index, const std::uint32_t vcount) override;
        void flush_to_driver(drivers::graphics_driver *driver, const bool is_frame_swap_flush = false) override;
        void destroy(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder) override;
        bool enable(const std::uint32_t feature) override;
        bool disable(const std::uint32_t feature) override;
        bool is_enabled(const std::uint32_t feature, bool &enabled) override;
        bool get_data(drivers::graphics_driver *drv, const std::uint32_t feature, void *data, gles_get_data_type data_type) override;
        std::uint32_t bind_texture(const std::uint32_t target, const std::uint32_t tex) override;
    };

    egl_context_es1 *get_es1_active_context(system *sys);

    /**
     * @brief Get the extension string for ES1 layer.
     * 
     * This also accounts the extensions that the host GPU supports.
     * 
     * @param   driver            Graphic driver pointer.
     * @return  The extension string.
     */
    std::string get_es1_extensions(drivers::graphics_driver *driver);
}