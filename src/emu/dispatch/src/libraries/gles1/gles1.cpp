/*
 * Copyright (c) 2021 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the free software Foundation, either version 3 of the License, or
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

#include <dispatch/libraries/gles1/gles1.h>
#include <dispatch/libraries/gles1/def.h>
#include <dispatch/libraries/gles_shared/consts.h>
#include <dispatch/libraries/gles_shared/utils.h>

#include <dispatch/dispatcher.h>
#include <drivers/graphics/graphics.h>
#include <system/epoc.h>
#include <kernel/kernel.h>

#include <cstdint>
#include <vector>

namespace eka2l1::dispatch {
    std::string get_es1_extensions(drivers::graphics_driver *driver) {
        std::string original_list = GLES1_STATIC_STRING_EXTENSIONS;
        if (driver->support_extension(drivers::graphics_driver_extension_anisotrophy_filtering)) {
            original_list += "GL_EXT_texture_filter_anisotropic ";
        }
        return original_list;
    }

    gles1_texture_unit::gles1_texture_unit()
        : index_(0)
        , binded_texture_handle_(0)
        , alpha_scale_(1)
        , rgb_scale_(1)
        , texturing_enabled_(false) {
        texture_mat_stack_.push(glm::identity<glm::mat4>());

        // Default all to modulates
        env_info_.env_mode_ = gles_texture_env_info::ENV_MODE_MODULATE;
        env_info_.src0_rgb_ = gles_texture_env_info::SOURCE_TYPE_CURRENT_TEXTURE;
        env_info_.src0_a_ = gles_texture_env_info::SOURCE_TYPE_CURRENT_TEXTURE;
        env_info_.src1_rgb_ = gles_texture_env_info::SOURCE_TYPE_PREVIOUS;
        env_info_.src1_a_ = gles_texture_env_info::SOURCE_TYPE_PREVIOUS;
        env_info_.src2_rgb_ = gles_texture_env_info::SOURCE_TYPE_CONSTANT;
        env_info_.src2_a_ = gles_texture_env_info::SOURCE_TYPE_CONSTANT;

        env_info_.src0_rgb_op_ = gles_texture_env_info::SOURCE_OPERAND_COLOR;
        env_info_.src1_rgb_op_ = gles_texture_env_info::SOURCE_OPERAND_COLOR;
        env_info_.src2_rgb_op_ = gles_texture_env_info::SOURCE_OPERAND_COLOR;

        env_info_.src0_a_op_ = gles_texture_env_info::SOURCE_OPERAND_ALPHA;
        env_info_.src1_a_op_ = gles_texture_env_info::SOURCE_OPERAND_ALPHA;
        env_info_.src2_a_op_ = gles_texture_env_info::SOURCE_OPERAND_ALPHA;

        env_info_.combine_rgb_func_ = gles_texture_env_info::SOURCE_COMBINE_MODULATE;
        env_info_.combine_a_func_ = gles_texture_env_info::SOURCE_COMBINE_MODULATE;

        env_colors_[0] = env_colors_[1] = env_colors_[2] = env_colors_[3] = 0.0f;
    }

    gles1_light_info::gles1_light_info()
        : spot_exponent_(0.0f)
        , spot_cutoff_(180.0f) {
        ambient_[0] = ambient_[1] = ambient_[2] = 0.0f;
        ambient_[3] = 1.0f;

        diffuse_[0] = diffuse_[1] = diffuse_[2] = diffuse_[3] = 0.0f;
        specular_[0] = specular_[1] = specular_[2] = specular_[3] = 0.0f;

        position_or_dir_[0] = position_or_dir_[1] = position_or_dir_[3] = 0.0f;
        position_or_dir_[2] = 1.0f;

        position_or_dir_transformed_[0] = position_or_dir_transformed_[1] = position_or_dir_transformed_[3] = 0.0f;
        position_or_dir_transformed_[2] = 1.0f; 

        spot_dir_[0] = spot_dir_[1] = 0.0f;
        spot_dir_[2] = -1.0f;

        spot_dir_transformed_[0] = spot_dir_transformed_[1] = 0.0f;
        spot_dir_transformed_[2] = -1.0f;

        attenuatation_[0] = 1.0f;
        attenuatation_[1] = attenuatation_[2] = 0.0f;
    }

    egl_context_es1::egl_context_es1()
        : egl_context_es_shared()
        , active_client_texture_unit_(0)
        , active_mat_stack_(GL_MODELVIEW_EMU)
        , current_palette_mat_index_(0)
        , previous_first_index_(0)
        , skin_weights_per_ver(GLES1_EMU_MAX_WEIGHTS_PER_VERTEX)
        , material_shininess_(0.0f)
        , fog_density_(1.0f)
        , fog_start_(0.0f)
        , fog_end_(1.0f)
        , vertex_statuses_(0)
        , fragment_statuses_(0)
        , alpha_test_ref_(0)
        , input_desc_(0) {
        material_ambient_[0] = 0.2f;
        material_ambient_[1] = 0.2f;
        material_ambient_[2] = 0.2f;
        material_ambient_[3] = 1.0f;

        material_diffuse_[0] = 0.8f;
        material_diffuse_[1] = 0.8f;
        material_diffuse_[2] = 0.8f;
        material_diffuse_[3] = 1.0f;

        material_specular_[0] = material_specular_[1] = material_specular_[2] = material_emission_[0]
            = material_emission_[1] = material_emission_[2] = 0.0f;

        material_specular_[3] = material_emission_[3] = 1.0f;

        // Fog
        fog_color_[0] = fog_color_[1] = fog_color_[2] = fog_color_[3] = 0.0f;

        // Lights
        lights_[0].diffuse_[0] = lights_[0].diffuse_[1] = lights_[0].diffuse_[2] = lights_[0].diffuse_[3] = 1.0f;
        lights_[0].specular_[0] = lights_[0].specular_[1] = lights_[0].specular_[2] = lights_[0].specular_[3] = 1.0f;

        global_ambient_[0] = global_ambient_[1] = global_ambient_[2] = 0.2f;
        global_ambient_[3] = 1.0f;

        // Clip planes
        for (std::size_t i = 0; i < GLES1_EMU_MAX_CLIP_PLANE; i++) {
            clip_planes_[i][0] = clip_planes_[i][1] = clip_planes_[i][2] = clip_planes_[i][3] = 0.0f;
            clip_planes_transformed_[i][0] = clip_planes_transformed_[i][1] = clip_planes_transformed_[i][2] = clip_planes_transformed_[i][3] = 0.0f;
        }

        // Uniforms
        color_uniforms_[0] = color_uniforms_[1] = color_uniforms_[2] = color_uniforms_[3] = 1.0f;

        // Attribs
        vertex_attrib_.size_ = 4;
        vertex_attrib_.stride_ = 0;
        vertex_attrib_.offset_ = 0;
        vertex_attrib_.data_type_ = GL_FLOAT_EMU;

        for (std::size_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
            texture_units_[i].coord_attrib_ = vertex_attrib_;
        }

        normal_attrib_ = vertex_attrib_;
        normal_attrib_.size_ = 3;

        color_attrib_ = vertex_attrib_;
        color_attrib_.size_ = 4;

        // Matrix
        model_view_mat_stack_.push(glm::identity<glm::mat4>());
        proj_mat_stack_.push(glm::identity<glm::mat4>());

        for (std::size_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
            texture_units_[i].index_ = i;
        }

        fragment_statuses_ |= ((GL_ALWAYS_EMU - GL_NEVER_EMU) << FRAGMENT_STATE_ALPHA_TEST_FUNC_POS) & FRAGMENT_STATE_ALPHA_FUNC_MASK;
        fragment_statuses_ |= FRAGMENT_STATE_FOG_MODE_EXP;
    }

    void egl_context_es1::flush_to_driver(drivers::graphics_driver *drv, const bool is_frame_swap_flush) {
        egl_context_es_shared::flush_to_driver(drv, is_frame_swap_flush);
    }

    void egl_context_es1::destroy(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder) {
        if (input_desc_) {
            cmd_builder_.destroy(input_desc_);
        }

        egl_context_es_shared::destroy(driver, builder);
    }

    glm::mat4 &egl_context_es1::active_matrix() {
        switch (active_mat_stack_) {
        case GL_MODELVIEW_EMU:
            return model_view_mat_stack_.top();

        case GL_PROJECTION_EMU:
            return proj_mat_stack_.top();

        case GL_TEXTURE_EMU:
            return texture_units_[active_texture_unit_].texture_mat_stack_.top();

        case GL_MATRIX_PALETTE_OES:
            return palette_mats_[current_palette_mat_index_];

        default:
            break;
        }

        return model_view_mat_stack_.top();
    }

    gles_driver_texture *egl_context_es1::binded_texture() {
        if (texture_units_[active_texture_unit_].binded_texture_handle_ == 0) {
            return nullptr;
        }

        std::uint32_t handle = texture_units_[active_texture_unit_].binded_texture_handle_;

        auto *obj = objects_.get(handle);
        if (!obj || !obj->get() || (*obj)->object_type() != GLES_OBJECT_TEXTURE) {
            if (!obj->get()) {
                // The capacity is still enough. Someone has deleted the texture that should not be ! (yes, Pet Me by mBounce)
                LOG_WARN(HLE_DISPATCHER, "Texture name {} was previously deleted, generate a new one"
                    " (only because the slot is empty)!", handle);
                *obj = std::make_unique<gles_driver_texture>(*this);
            } else {
                return nullptr;
            }
        }

        return reinterpret_cast<gles_driver_texture*>(obj->get());
    }

    bool egl_context_es1::enable(const std::uint32_t feature) {
        if (egl_context_es_shared::enable(feature)) {
            return true;
        }

        switch (feature) {
        case GL_ALPHA_TEST_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_ALPHA_TEST;
            break;

        case GL_CLIP_PLANE0_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE0_ENABLE;
            break;
            
        case GL_CLIP_PLANE1_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE1_ENABLE;
            break;

        case GL_CLIP_PLANE2_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE2_ENABLE;
            break;

        case GL_CLIP_PLANE3_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE3_ENABLE;
            break;

        case GL_CLIP_PLANE4_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE4_ENABLE;
            break;

        case GL_CLIP_PLANE5_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE5_ENABLE;
            break;

        case GL_COLOR_MATERIAL_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_COLOR_MATERIAL_ENABLE;
            break;

        case GL_FOG_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_FOG_ENABLE;
            break;

        case GL_LIGHTING_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHTING_ENABLE;
            break;

        case GL_LIGHT0_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT0_ON;
            break;

        case GL_LIGHT1_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT1_ON;
            break;

        case GL_LIGHT2_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT2_ON;
            break;

        case GL_LIGHT3_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT3_ON;
            break;

        case GL_LIGHT4_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT4_ON;
            break;

        case GL_LIGHT5_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT5_ON;
            break;

        case GL_LIGHT6_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT6_ON;
            break;
        
        case GL_LIGHT7_EMU:
            fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT7_ON;
            break;

        case GL_TEXTURE_2D_EMU:
            texture_units_[active_texture_unit_].texturing_enabled_ = true;
            break;

        case GL_NORMALIZE_EMU:
            vertex_statuses_ |= egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_NORMALIZE;
            break;

        case GL_RESCALE_NORMAL_EMU:
            vertex_statuses_ |= egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_RESCALE;
            break;

        case GL_MATRIX_PALETTE_OES:
            vertex_statuses_ |= egl_context_es1::VERTEX_STATE_SKINNING_ENABLE;
            break;

        default:
            return false;
        }

        return true;
    }

    bool egl_context_es1::disable(const std::uint32_t feature) {
        if (egl_context_es_shared::disable(feature)) {
            return true;
        }

        switch (feature) {
        case GL_ALPHA_TEST_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_ALPHA_TEST;
            break;

        case GL_CLIP_PLANE0_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE0_ENABLE;
            break;
            
        case GL_CLIP_PLANE1_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE1_ENABLE;
            break;

        case GL_CLIP_PLANE2_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE2_ENABLE;
            break;

        case GL_CLIP_PLANE3_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE3_ENABLE;
            break;

        case GL_CLIP_PLANE4_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE4_ENABLE;
            break;

        case GL_CLIP_PLANE5_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE5_ENABLE;
            break;

        case GL_COLOR_MATERIAL_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_COLOR_MATERIAL_ENABLE;
            break;

        case GL_FOG_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_FOG_ENABLE;
            break;

        case GL_LIGHTING_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHTING_ENABLE;
            break;

        case GL_LIGHT0_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT0_ON;
            break;

        case GL_LIGHT1_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT1_ON;
            break;

        case GL_LIGHT2_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT2_ON;
            break;

        case GL_LIGHT3_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT3_ON;
            break;

        case GL_LIGHT4_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT4_ON;
            break;

        case GL_LIGHT5_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT5_ON;
            break;

        case GL_LIGHT6_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT6_ON;
            break;
        
        case GL_LIGHT7_EMU:
            fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT7_ON;
            break;

        case GL_TEXTURE_2D_EMU:
            texture_units_[active_texture_unit_].texturing_enabled_ = false;
            break;

        case GL_NORMALIZE_EMU:
            vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_NORMALIZE;
            break;

        case GL_RESCALE_NORMAL_EMU:
            vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_RESCALE;
            break;

        case GL_MATRIX_PALETTE_OES:
            vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_SKINNING_ENABLE;
            break;

        default:
            return false;
        }

        return true;
    }

    bool egl_context_es1::is_enabled(const std::uint32_t feature, bool &enabled) {
        if (egl_context_es_shared::is_enabled(feature, enabled)) {
            return true;
        }

        switch (feature) {
        case GL_ALPHA_TEST_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_ALPHA_TEST) ? 1 : 0;
            break;

        case GL_CLIP_PLANE0_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_CLIP_PLANE0_ENABLE) ? 1 : 0;
            break;

        case GL_CLIP_PLANE1_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_CLIP_PLANE1_ENABLE) ? 1 : 0;
            break;

        case GL_CLIP_PLANE2_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_CLIP_PLANE2_ENABLE) ? 1 : 0;
            break;

        case GL_CLIP_PLANE3_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_CLIP_PLANE3_ENABLE) ? 1 : 0;
            break;

        case GL_CLIP_PLANE4_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_CLIP_PLANE4_ENABLE) ? 1 : 0;
            break;

        case GL_CLIP_PLANE5_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_CLIP_PLANE5_ENABLE) ? 1 : 0;
            break;

        case GL_COLOR_MATERIAL_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_COLOR_MATERIAL_ENABLE) ? 1 : 0;
            break;

        case GL_FOG_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_FOG_ENABLE) ? 1 : 0;
            break;

        case GL_LIGHTING_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHTING_ENABLE) ? 1 : 0;
            break;

        case GL_LIGHT0_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT0_ON) ? 1 : 0;
            break;

        case GL_LIGHT1_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT1_ON) ? 1 : 0;
            break;

        case GL_LIGHT2_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT2_ON) ? 1 : 0;
            break;

        case GL_LIGHT3_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT3_ON) ? 1 : 0;
            break;

        case GL_LIGHT4_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT4_ON) ? 1 : 0;
            break;

        case GL_LIGHT5_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT5_ON) ? 1 : 0;
            break;

        case GL_LIGHT6_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT6_ON) ? 1 : 0;
            break;
        
        case GL_LIGHT7_EMU:
            enabled = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT7_ON) ? 1 : 0;
            break;

        case GL_TEXTURE_2D_EMU:
            enabled = static_cast<std::int32_t>(texture_units_[active_texture_unit_].texturing_enabled_);
            break;

        case GL_NORMALIZE_EMU:
            enabled = (vertex_statuses_ & egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_NORMALIZE) ? 1 : 0;
            break;

        case GL_RESCALE_NORMAL_EMU:
            enabled = (vertex_statuses_ & egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_RESCALE) ? 1 : 0;
            break;

        case GL_VERTEX_ARRAY_EMU:
            enabled = (vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_VERTEX_ARRAY) ? 1 : 0;
            break;

        case GL_COLOR_ARRAY_EMU:
            enabled = (vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY) ? 1 : 0;
            break;

        case GL_NORMAL_ARRAY_EMU:
            enabled = (vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY) ? 1 : 0;
            break;

        case GL_TEXTURE_COORD_ARRAY_EMU: {
            std::uint64_t mask = 1 << (egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD_ARRAY_POS +
                static_cast<std::uint8_t>(active_client_texture_unit_));
            enabled = (vertex_statuses_ & mask) ? 1 : 0;
            break;
        }

        default:
            return false;
        }

        return true;
    }

    template <typename T>
    bool get_data_impl_es1(egl_context_es1 *ctx, drivers::graphics_driver *drv, std::uint32_t pname, T *params, std::uint32_t scale_factor) {
        switch (pname) {
        case GL_CLIENT_ACTIVE_TEXTURE_EMU:
            *params = static_cast<T>(ctx->active_client_texture_unit_ + GL_TEXTURE0_EMU);
            break;

        case GL_ALPHA_TEST_EMU:
            if (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_ALPHA_TEST)
                *params = 1;
            else
                *params = 0;

            break;

        case GL_ALPHA_TEST_FUNC_EMU:
            *params = static_cast<T>(((ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_ALPHA_FUNC_MASK)
                >> egl_context_es1::FRAGMENT_STATE_ALPHA_TEST_FUNC_POS) + GL_NEVER_EMU);
            break;

        case GL_ALPHA_TEST_REF_EMU:
            *params = static_cast<T>(ctx->alpha_test_ref_);
            break;

        case GL_CLIP_PLANE0_EMU:
        case GL_CLIP_PLANE1_EMU:
        case GL_CLIP_PLANE2_EMU:
        case GL_CLIP_PLANE3_EMU:
        case GL_CLIP_PLANE4_EMU:
        case GL_CLIP_PLANE5_EMU:
            *params = static_cast<T>((ctx->fragment_statuses_ & (1 << (egl_context_es1::FRAGMENT_STATE_CLIP_PLANE_BIT_POS +
                pname - GL_CLIP_PLANE0_EMU))) ? 1 : 0);
            break;

        case GL_COLOR_ARRAY_EMU:
            if (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY)
                *params = 1;
            else
                *params = 0;

            break;

        case GL_COLOR_ARRAY_BUFFER_BINDING_EMU:
            *params = static_cast<T>(ctx->color_attrib_.buffer_obj_);
            break;

        case GL_COLOR_ARRAY_SIZE_EMU:
            *params = static_cast<T>(ctx->color_attrib_.size_);
            break;

        case GL_COLOR_ARRAY_STRIDE_EMU:
            *params = static_cast<T>(ctx->color_attrib_.stride_);
            break;

        case GL_COLOR_ARRAY_TYPE_EMU:
            *params = static_cast<T>(ctx->color_attrib_.data_type_);
            break;

        case GL_COLOR_MATERIAL_EMU:
            *params = static_cast<T>((ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_COLOR_MATERIAL_ENABLE) ? 1 : 0);
            break;

        case GL_CURRENT_COLOR_EMU:
            params[0] = static_cast<T>(ctx->color_uniforms_[0] * scale_factor);
            params[1] = static_cast<T>(ctx->color_uniforms_[1] * scale_factor);
            params[2] = static_cast<T>(ctx->color_uniforms_[2] * scale_factor);
            params[3] = static_cast<T>(ctx->color_uniforms_[3] * scale_factor);
            break;

        case GL_CURRENT_NORMAL_EMU:
            params[0] = static_cast<T>(ctx->normal_uniforms_[0]);
            params[1] = static_cast<T>(ctx->normal_uniforms_[1]);
            params[2] = static_cast<T>(ctx->normal_uniforms_[2]);

            break;

        case GL_CURRENT_TEXTURE_COORDS_EMU:
            params[0] = static_cast<T>(ctx->texture_units_[ctx->active_texture_unit_].coord_uniforms_[0]);
            params[1] = static_cast<T>(ctx->texture_units_[ctx->active_texture_unit_].coord_uniforms_[1]);
            params[2] = static_cast<T>(ctx->texture_units_[ctx->active_texture_unit_].coord_uniforms_[2]);
            params[3] = static_cast<T>(ctx->texture_units_[ctx->active_texture_unit_].coord_uniforms_[3]);
            break;

        case GL_MATRIX_MODE_EMU:
            *params = static_cast<T>(ctx->active_mat_stack_);
            break;

        case GL_NORMAL_ARRAY_EMU:
            *params = static_cast<T>((ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY) ? 1 : 0);
            break;

        case GL_NORMAL_ARRAY_BUFFER_BINDING_EMU:
            *params = static_cast<T>(ctx->normal_attrib_.buffer_obj_);
            break;

        case GL_NORMAL_ARRAY_STRIDE_EMU:
            *params = static_cast<T>(ctx->normal_attrib_.stride_);
            break;
        
        case GL_NORMAL_ARRAY_TYPE_EMU:
            *params = static_cast<T>(ctx->normal_attrib_.data_type_);
            break;

        case GL_NORMAL_ARRAY_POINTER_EMU:
            *params = static_cast<T>(ctx->normal_attrib_.offset_);
            break;

        case GL_TEXTURE_COORD_ARRAY_EMU: {
            std::uint64_t mask = 1 << static_cast<std::uint8_t>(egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD_ARRAY_POS +
                static_cast<std::uint8_t>(ctx->active_client_texture_unit_));

            *params = static_cast<T>((ctx->vertex_statuses_ & mask) ? 1 : 0);
            break;
        }

        case GL_VERTEX_ARRAY_BUFFER_BINDING_EMU:
            *params = static_cast<T>(ctx->vertex_attrib_.buffer_obj_);
            break;

        case GL_VERTEX_ARRAY_STRIDE_EMU:
            *params = static_cast<T>(ctx->vertex_attrib_.stride_);
            break;

        case GL_VERTEX_ARRAY_SIZE_EMU:
            *params = static_cast<T>(ctx->vertex_attrib_.size_);
            break;
        
        case GL_VERTEX_ARRAY_TYPE_EMU:
            *params = static_cast<T>(ctx->vertex_attrib_.data_type_);
            break;

        case GL_VERTEX_ARRAY_POINTER_EMU:
            *params = static_cast<T>(ctx->vertex_attrib_.offset_);
            break;

        case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_EMU:
            *params = static_cast<T>(ctx->texture_units_[ctx->active_texture_unit_].coord_attrib_.buffer_obj_);
            break;

        case GL_TEXTURE_COORD_ARRAY_SIZE_EMU:
            *params = static_cast<T>(ctx->texture_units_[ctx->active_texture_unit_].coord_attrib_.size_);
            break;

        case GL_TEXTURE_COORD_ARRAY_STRIDE_EMU:
            *params = static_cast<T>(ctx->texture_units_[ctx->active_texture_unit_].coord_attrib_.stride_);
            break;

        case GL_TEXTURE_COORD_ARRAY_TYPE_EMU:
            *params = static_cast<T>(ctx->texture_units_[ctx->active_texture_unit_].coord_attrib_.data_type_);
            break;

        case GL_MAX_CLIP_PLANES_EMU:
            *params = GLES1_EMU_MAX_CLIP_PLANE;
            break;

        case GL_MAX_LIGHTS_EMU:
            *params = GLES1_EMU_MAX_LIGHT;
            break;

        case GL_RESCALE_NORMAL_EMU:
            *params = static_cast<T>((ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_RESCALE) ? 1 : 0);
            break;

        case GL_NORMALIZE_EMU:
            *params = static_cast<T>((ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_NORMALIZE) ? 1 : 0);
            break;

        case GL_MODELVIEW_MATRIX_EMU: {
            glm::mat4 &mat = ctx->model_view_mat_stack_.top();
            float *mat_ptr = glm::value_ptr(mat);
            for (std::uint32_t i = 0; i < 16; i++) {
                params[i] = static_cast<T>(mat_ptr[i]);
            }

            break;
        }

        case GL_PROJECTION_MATRIX_EMU: {
            glm::mat4 &mat = ctx->proj_mat_stack_.top();
            float *mat_ptr = glm::value_ptr(mat);
            for (std::uint32_t i = 0; i < 16; i++) {
                params[i] = static_cast<T>(mat_ptr[i]);
            }

            break;
        }
        
        case GL_TEXTURE_MATRIX_EMU: {
            glm::mat4 &mat = ctx->texture_units_[ctx->active_texture_unit_].texture_mat_stack_.top();
            float *mat_ptr = glm::value_ptr(mat);
            for (std::uint32_t i = 0; i < 16; i++) {
                params[i] = static_cast<T>(mat_ptr[i]);
            }

            break;
        }
        
        case GL_MODELVIEW_STACK_DEPTH_EMU:
            *params = static_cast<T>(ctx->model_view_mat_stack_.size());
            break;

        case GL_PROJECTION_STACK_DEPTH_EMU:
            *params = static_cast<T>(ctx->proj_mat_stack_.size());
            break;

        case GL_TEXTURE_STACK_DEPTH_EMU:
            *params = static_cast<T>(ctx->texture_units_[ctx->active_texture_unit_].texture_mat_stack_.size());
            break;

        case GL_MAX_MODELVIEW_STACK_DEPTH_EMU:
            *params = static_cast<T>(GL_MAXIMUM_MODELVIEW_MATRIX_STACK_SIZE);
            break;

        case GL_MAX_PROJECTION_STACK_DEPTH_EMU:
            *params = static_cast<T>(GL_MAXIMUM_PROJECTION_MATRIX_STACK_SIZE);
            break;

        case GL_MAX_TEXTURE_STACK_DEPTH_EMU:
            *params = static_cast<T>(GL_MAXIMUM_TEXTURE_MATRIX_STACK_SIZE);
            break;

        case GL_MAX_TEXTURE_UNITS_EMU:
            *params = static_cast<T>(GLES1_EMU_MAX_TEXTURE_COUNT);
            break;

        case GL_SUBPIXEL_BITS_EMU:
            *params = static_cast<T>(4);
            break;

        case GL_RED_BITS_EMU:
            *params = static_cast<T>(ctx->draw_surface_->config_.red_bits());
            break;

        case GL_GREEN_BITS_EMU:
            *params = static_cast<T>(ctx->draw_surface_->config_.green_bits());
            break;

        case GL_BLUE_BITS_EMU:
            *params = static_cast<T>(ctx->draw_surface_->config_.blue_bits());
            break;

        case GL_ALPHA_BITS_EMU:
            *params = static_cast<T>(ctx->draw_surface_->config_.alpha_bits());
            break;

        case GL_DEPTH_BITS_EMU:
            *params = static_cast<T>(24);
            break;

        case GL_STENCIL_BITS_EMU:
            *params = static_cast<T>(8);
            break;

        case GL_MAX_PALETTE_MATRICES_OES:
            *params = static_cast<T>(GLES1_EMU_MAX_PALETTE_MATRICES);
            break;

        case GL_MAX_VERTEX_UNIT_OES:
            *params = static_cast<T>(GLES1_EMU_MAX_WEIGHTS_PER_VERTEX);
            break;

        case GL_CURRENT_PALETTE_MATRIX_OES:
            *params = static_cast<T>(ctx->current_palette_mat_index_);
            break;

        default:
            return false;
        }

        return true;
    }

    bool egl_context_es1::get_data(drivers::graphics_driver *drv, const std::uint32_t feature, void *data, gles_get_data_type data_type) {
        if (egl_context_es_shared::get_data(drv, feature, data, data_type)) {
            return true;
        }
        switch (data_type) {
        case GLES_GET_DATA_TYPE_BOOLEAN:
            return get_data_impl_es1<std::int32_t>(this, drv, feature, reinterpret_cast<std::int32_t*>(data), 255);

        case GLES_GET_DATA_TYPE_FIXED:
            return get_data_impl_es1<gl_fixed>(this, drv, feature, reinterpret_cast<gl_fixed*>(data), 65536);

        case GLES_GET_DATA_TYPE_FLOAT:
            return get_data_impl_es1<float>(this, drv, feature, reinterpret_cast<float*>(data), 1);

        case GLES_GET_DATA_TYPE_INTEGER:
            return get_data_impl_es1<std::uint32_t>(this, drv, feature, reinterpret_cast<std::uint32_t*>(data), 255);

        default:
            break;
        }

        return false;
    }

    std::uint32_t egl_context_es1::bind_texture(const std::uint32_t target, const std::uint32_t tex) {
        auto *obj = objects_.get(tex);
        if (obj && (*obj).get()) {
            std::uint32_t bind_res = reinterpret_cast<gles_driver_texture*>((*obj).get())->try_bind(target);
            if (bind_res != 0) {
                return bind_res;
            }
        }

        texture_units_[active_texture_unit_].binded_texture_handle_ = tex;
        return 0;
    }

    egl_context_es1 *get_es1_active_context(system *sys) {
        if (!sys) {
            return nullptr;
        }

        dispatcher *dp = sys->get_dispatcher();
        if (!dp) {
            return nullptr;
        }

        kernel_system *kern = sys->get_kernel_system();
        if (!kern) {
            return nullptr;
        }

        kernel::thread *crr_thread = kern->crr_thread();
        if (!crr_thread) {
            return nullptr;
        }

        dispatch::egl_controller &controller = dp->get_egl_controller();
        kernel::uid crr_thread_uid = crr_thread->unique_id();

        egl_context *context = controller.current_context(crr_thread_uid);
        if (!context || (context->context_type() != EGL_GLES1_CONTEXT)) {
            controller.push_error(crr_thread_uid, GL_INVALID_OPERATION);
            return nullptr;
        }

        return reinterpret_cast<egl_context_es1*>(context);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_matrix_mode_emu, std::uint32_t mode) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();

        if ((mode != GL_TEXTURE_EMU) && (mode != GL_MODELVIEW_EMU) && (mode != GL_PROJECTION_EMU)
            && (mode != GL_MATRIX_PALETTE_OES)) {
            dp->get_egl_controller().push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        ctx->active_mat_stack_ = mode;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_push_matrix_emu) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (ctx->active_mat_stack_) {
        case GL_MODELVIEW_EMU:
            if (ctx->model_view_mat_stack_.size() >= GL_MAXIMUM_MODELVIEW_MATRIX_STACK_SIZE) {
                controller.push_error(ctx, GL_STACK_OVERFLOW);
                return;
            }

            ctx->model_view_mat_stack_.push(ctx->model_view_mat_stack_.top());
            break;

        case GL_PROJECTION_EMU:
            if (ctx->proj_mat_stack_.size() >= GL_MAXIMUM_PROJECTION_MATRIX_STACK_SIZE) {
                controller.push_error(ctx, GL_STACK_OVERFLOW);
                return;
            }

            ctx->proj_mat_stack_.push(ctx->proj_mat_stack_.top());
            break;

        case GL_TEXTURE_EMU: {
            auto &ss = ctx->texture_units_[ctx->active_texture_unit_].texture_mat_stack_;

            if (ss.size() >= GL_MAXIMUM_TEXTURE_MATRIX_STACK_SIZE) {
                controller.push_error(ctx, GL_STACK_OVERFLOW);
                return;
            }

            ss.push(ss.top());
            break;
        }

        default:
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_pop_matrix_emu) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (ctx->active_mat_stack_) {
        case GL_MODELVIEW_EMU:
            if (ctx->model_view_mat_stack_.size() == 1) {
                controller.push_error(ctx, GL_STACK_UNDERFLOW);
                return;
            }

            ctx->model_view_mat_stack_.pop();
            break;

        case GL_PROJECTION_EMU:
            if (ctx->proj_mat_stack_.size() == 1) {
                controller.push_error(ctx, GL_STACK_UNDERFLOW);
                return;
            }

            ctx->proj_mat_stack_.pop();
            break;

        case GL_TEXTURE_EMU:
            if (ctx->texture_units_[ctx->active_texture_unit_].texture_mat_stack_.size() == 1) {
                controller.push_error(ctx, GL_STACK_UNDERFLOW);
                return;
            }

            ctx->texture_units_[ctx->active_texture_unit_].texture_mat_stack_.pop();
            break;

        default:
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_load_identity_emu) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::identity<glm::mat4>();
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_load_matrixf_emu, float *mat) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!mat) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        // Column wise, so is good
        ctx->active_matrix() = glm::make_mat4(mat);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_load_matrixx_emu, gl_fixed *mat) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!mat) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        float converted[16];
        for (int i = 0; i < 16; i++) {
            converted[i] = FIXED_32_TO_FLOAT(mat[i]);
        }

        // Column wise, so is good
        ctx->active_matrix() = glm::make_mat4(converted);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_orthof_emu, float left, float right, float bottom, float top, float near, float far) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() *= glm::ortho(left, right, bottom, top, near, far);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_orthox_emu, gl_fixed left, gl_fixed right, gl_fixed bottom, gl_fixed top, gl_fixed near, gl_fixed far) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() *= glm::ortho(FIXED_32_TO_FLOAT(left), FIXED_32_TO_FLOAT(right), FIXED_32_TO_FLOAT(bottom), FIXED_32_TO_FLOAT(top),
            FIXED_32_TO_FLOAT(near), FIXED_32_TO_FLOAT(far));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_mult_matrixf_emu, float *m) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!m) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        ctx->active_matrix() *= glm::make_mat4(m);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_mult_matrixx_emu, gl_fixed *m) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!m) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        float converted[16];
        for (int i = 0; i < 16; i++) {
            converted[i] = FIXED_32_TO_FLOAT(m[i]);
        }

        ctx->active_matrix() *= glm::make_mat4(converted);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_scalef_emu, float x, float y, float z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::scale(ctx->active_matrix(), glm::vec3(x, y, z));
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_scalex_emu, gl_fixed x, gl_fixed y, gl_fixed z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::scale(ctx->active_matrix(), glm::vec3(FIXED_32_TO_FLOAT(x), FIXED_32_TO_FLOAT(y), FIXED_32_TO_FLOAT(z)));
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_translatef_emu, float x, float y, float z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::translate(ctx->active_matrix(), glm::vec3(x, y, z));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_translatex_emu, gl_fixed x, gl_fixed y, gl_fixed z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::translate(ctx->active_matrix(), glm::vec3(FIXED_32_TO_FLOAT(x), FIXED_32_TO_FLOAT(y), FIXED_32_TO_FLOAT(z)));
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_rotatef_emu, float angles, float x, float y, float z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::rotate(ctx->active_matrix(), glm::radians(angles), glm::vec3(x, y, z));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_rotatex_emu, gl_fixed angles, gl_fixed x, gl_fixed y, gl_fixed z) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() = glm::rotate(ctx->active_matrix(), glm::radians(FIXED_32_TO_FLOAT(angles)), glm::vec3(FIXED_32_TO_FLOAT(x), FIXED_32_TO_FLOAT(y), FIXED_32_TO_FLOAT(z)));
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_frustumf_emu, float left, float right, float bottom, float top, float near, float far) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() *= glm::frustum(left, right, bottom, top, near, far);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_frustumx_emu, gl_fixed left, gl_fixed right, gl_fixed bottom, gl_fixed top, gl_fixed near, gl_fixed far) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->active_matrix() *= glm::frustum(FIXED_32_TO_FLOAT(left), FIXED_32_TO_FLOAT(right), FIXED_32_TO_FLOAT(bottom), FIXED_32_TO_FLOAT(top),
            FIXED_32_TO_FLOAT(near), FIXED_32_TO_FLOAT(far));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_alpha_func_emu, std::uint32_t func, float ref) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!is_valid_gl_emu_func(func)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_ALPHA_FUNC_MASK;
        ctx->fragment_statuses_ |= ((func - GL_NEVER_EMU) << egl_context_es1::FRAGMENT_STATE_ALPHA_TEST_FUNC_POS)
            & egl_context_es1::FRAGMENT_STATE_ALPHA_FUNC_MASK;

        ctx->alpha_test_ref_ = ref;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_alpha_func_x_emu, std::uint32_t func, gl_fixed ref) {
        gl_alpha_func_emu(sys, func, FIXED_32_TO_FLOAT(ref));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_shade_model_emu, std::uint32_t model) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (model == GL_FLAT_EMU) {
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_SHADE_MODEL_FLAT;
        } else if (model == GL_SMOOTH_EMU) {
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_SHADE_MODEL_FLAT;
        } else {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_normal_3f_emu, float nx, float ny, float nz) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->normal_uniforms_[0] = nx;
        ctx->normal_uniforms_[1] = ny;
        ctx->normal_uniforms_[2] = nz;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_normal_3x_emu, gl_fixed nx, gl_fixed ny, gl_fixed nz) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->normal_uniforms_[0] = FIXED_32_TO_FLOAT(nx);
        ctx->normal_uniforms_[1] = FIXED_32_TO_FLOAT(ny);
        ctx->normal_uniforms_[2] = FIXED_32_TO_FLOAT(nz);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_color_4f_emu, float red, float green, float blue, float alpha) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->color_uniforms_[0] = red;
        ctx->color_uniforms_[1] = green;
        ctx->color_uniforms_[2] = blue;
        ctx->color_uniforms_[3] = alpha;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_color_4x_emu, gl_fixed red, gl_fixed green, gl_fixed blue, gl_fixed alpha) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->color_uniforms_[0] = FIXED_32_TO_FLOAT(red);
        ctx->color_uniforms_[1] = FIXED_32_TO_FLOAT(green);
        ctx->color_uniforms_[2] = FIXED_32_TO_FLOAT(blue);
        ctx->color_uniforms_[3] = FIXED_32_TO_FLOAT(alpha);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_color_4ub_emu, std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->color_uniforms_[0] = red / 255.0f;
        ctx->color_uniforms_[1] = green / 255.0f;
        ctx->color_uniforms_[2] = blue / 255.0f;
        ctx->color_uniforms_[3] = alpha / 255.0f;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_client_active_texture_emu, std::uint32_t unit) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((unit < GL_TEXTURE0_EMU) && (unit >= GL_TEXTURE0_EMU + GLES1_EMU_MAX_TEXTURE_COUNT)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->active_client_texture_unit_ = unit - GL_TEXTURE0_EMU;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_multi_tex_coord_4f_emu, std::uint32_t unit, float s, float t, float r, float q) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((unit < GL_TEXTURE0_EMU) && (unit >= GL_TEXTURE0_EMU + GLES1_EMU_MAX_TEXTURE_COUNT)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        unit -= GL_TEXTURE0_EMU;

        ctx->texture_units_[unit].coord_uniforms_[0] = s;
        ctx->texture_units_[unit].coord_uniforms_[1] = t;
        ctx->texture_units_[unit].coord_uniforms_[2] = r;
        ctx->texture_units_[unit].coord_uniforms_[3] = q;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_multi_tex_coord_4x_emu, std::uint32_t unit, gl_fixed s, gl_fixed t, gl_fixed r, gl_fixed q) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((unit < GL_TEXTURE0_EMU) && (unit >= GL_TEXTURE0_EMU + GLES1_EMU_MAX_TEXTURE_COUNT)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->texture_units_[unit].coord_uniforms_[0] = FIXED_32_TO_FLOAT(s);
        ctx->texture_units_[unit].coord_uniforms_[1] = FIXED_32_TO_FLOAT(t);
        ctx->texture_units_[unit].coord_uniforms_[2] = FIXED_32_TO_FLOAT(r);
        ctx->texture_units_[unit].coord_uniforms_[3] = FIXED_32_TO_FLOAT(q);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_enable_client_state_emu, std::uint32_t state) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (state) {
        case GL_VERTEX_ARRAY_EMU:
            ctx->vertex_statuses_ |= egl_context_es1::VERTEX_STATE_CLIENT_VERTEX_ARRAY;
            break;

        case GL_COLOR_ARRAY_EMU:
            ctx->vertex_statuses_ |= egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY;
            break;

        case GL_NORMAL_ARRAY_EMU:
            ctx->vertex_statuses_ |= egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY;
            break;

        case GL_TEXTURE_COORD_ARRAY_EMU:
            ctx->vertex_statuses_ |= (1 << (egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD_ARRAY_POS +
                static_cast<std::uint8_t>(ctx->active_client_texture_unit_)));

            break;

        case GL_WEIGHT_ARRAY_OES:
            ctx->vertex_statuses_ |= egl_context_es1::VERTEX_STATE_CLIENT_WEIGHT_ARRAY;
            break;

        case GL_MATRIX_INDEX_ARRAY_OES:
            ctx->vertex_statuses_ |= egl_context_es1::VERTEX_STATE_CLIENT_MATRIX_INDEX_ARRAY;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_disable_client_state_emu, std::uint32_t state) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (state) {
        case GL_VERTEX_ARRAY_EMU:
            ctx->vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_CLIENT_VERTEX_ARRAY;
            break;

        case GL_COLOR_ARRAY_EMU:
            ctx->vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY;
            break;

        case GL_NORMAL_ARRAY_EMU:
            ctx->vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY;
            break;

        case GL_TEXTURE_COORD_ARRAY_EMU:
            ctx->vertex_statuses_ &= ~(1 << (egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD_ARRAY_POS +
                static_cast<std::uint8_t>(ctx->active_client_texture_unit_)));
            break;
            
        case GL_WEIGHT_ARRAY_OES:
            ctx->vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_CLIENT_WEIGHT_ARRAY;
            break;

        case GL_MATRIX_INDEX_ARRAY_OES:
            ctx->vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_CLIENT_MATRIX_INDEX_ARRAY;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_color_pointer_emu, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((size != 4) || (stride < 0)) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (!assign_vertex_attrib_gles(ctx->color_attrib_, size, type, stride, offset, ctx->binded_array_buffer_handle_)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_normal_pointer_emu, std::uint32_t type, std::int32_t stride, std::uint32_t offset) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (stride < 0) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (!assign_vertex_attrib_gles(ctx->normal_attrib_, 3, type, stride, offset, ctx->binded_array_buffer_handle_)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_vertex_pointer_emu, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((stride < 0) || ((size != 2) && (size != 3) && (size != 4))) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (!assign_vertex_attrib_gles(ctx->vertex_attrib_, size, type, stride, offset, ctx->binded_array_buffer_handle_)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_texcoord_pointer_emu, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((stride < 0) || ((size != 2) && (size != 3) && (size != 4))) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (!assign_vertex_attrib_gles(ctx->texture_units_[ctx->active_client_texture_unit_].coord_attrib_, size, type, stride, offset, ctx->binded_array_buffer_handle_)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_material_f_emu, std::uint32_t target, std::uint32_t pname, const float pvalue) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_FRONT_AND_BACK_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (pname == GL_SHININESS_EMU) {
            if ((pvalue < 0.0f) || (pvalue > 128.0f)) {
                controller.push_error(ctx, GL_INVALID_VALUE);
                return;
            }

            ctx->material_shininess_ = pvalue;
        } else {
            controller.push_error(ctx, GL_INVALID_ENUM);
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_material_x_emu, std::uint32_t target, std::uint32_t pname, const gl_fixed pvalue) {
        gl_material_f_emu(sys, target, pname, FIXED_32_TO_FLOAT(pvalue));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_material_fv_emu, std::uint32_t target, std::uint32_t pname, const float *pvalue) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        float *dest_params = nullptr;
        float *dest_params_2 = nullptr;

        switch (pname) {
        case GL_AMBIENT_EMU:
            dest_params = ctx->material_ambient_;
            break;

        case GL_DIFFUSE_EMU:
            dest_params = ctx->material_diffuse_;
            break;

        case GL_SPECULAR_EMU:
            dest_params = ctx->material_specular_;
            break;

        case GL_EMISSION_EMU:
            dest_params = ctx->material_emission_;
            break;

        case GL_SHININESS_EMU:
            ctx->material_shininess_ = *pvalue;
            return;

        case GL_AMBIENT_AND_DIFFUSE_EMU:
            dest_params = ctx->material_ambient_;
            dest_params_2 = ctx->material_diffuse_;

            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (dest_params) {
            std::memcpy(dest_params, pvalue, 4 * sizeof(float));
        }

        if (dest_params_2) {
            std::memcpy(dest_params_2, pvalue, 4 * sizeof(float));
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_material_xv_emu, std::uint32_t target, std::uint32_t pname, const gl_fixed *pvalue) {
        float converted[4];
        converted[0] = FIXED_32_TO_FLOAT(pvalue[0]);
        converted[1] = FIXED_32_TO_FLOAT(pvalue[1]);
        converted[2] = FIXED_32_TO_FLOAT(pvalue[2]);
        converted[3] = FIXED_32_TO_FLOAT(pvalue[3]);

        gl_material_fv_emu(sys, target, pname, converted);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_fog_f_emu, std::uint32_t target, const float param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (target) {
        case GL_FOG_START_EMU:
            ctx->fog_start_ = param;
            break;

        case GL_FOG_END_EMU:
            ctx->fog_end_ = param;
            break;

        case GL_FOG_DENSITY_EMU:
            ctx->fog_density_ = param;
            break;

        case GL_FOG_MODE_EMU: {
            const std::uint32_t mode_u32 = static_cast<const std::uint32_t>(param);
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_FOG_MODE_MASK;
            if (mode_u32 == GL_LINEAR_EMU) {
                ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_FOG_MODE_LINEAR;
            } else if (mode_u32 == GL_EXP_EMU) {
                ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_FOG_MODE_EXP;
            } else if (mode_u32 == GL_EXP2_EMU) {
                ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_FOG_MODE_EXP2;
            } else {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            break;
        }

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_fog_x_emu, std::uint32_t target, const gl_fixed param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (target) {
        case GL_FOG_MODE_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_FOG_MODE_MASK;
            if (param == GL_LINEAR_EMU) {
                ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_FOG_MODE_LINEAR;
            } else if (param == GL_EXP_EMU) {
                ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_FOG_MODE_EXP;
            } else if (param == GL_EXP2_EMU) {
                ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_FOG_MODE_EXP2;
            } else {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            break;

        case GL_FOG_START_EMU:
            ctx->fog_start_ = FIXED_32_TO_FLOAT(param);
            break;

        case GL_FOG_END_EMU:
            ctx->fog_end_ = FIXED_32_TO_FLOAT(param);
            break;

        case GL_FOG_DENSITY_EMU:
            ctx->fog_density_ = FIXED_32_TO_FLOAT(param);
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_fog_fv_emu, std::uint32_t target, const float *param) {
        if ((target == GL_FOG_START_EMU) || (target == GL_FOG_END_EMU) || (target == GL_FOG_DENSITY_EMU)) {
            gl_fog_f_emu(sys, target, *param);
            return;
        }
        
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target == GL_FOG_COLOR_EMU) {
            ctx->fog_color_[0] = param[0];
            ctx->fog_color_[1] = param[1];
            ctx->fog_color_[2] = param[2];
            ctx->fog_color_[3] = param[3];
        } else {
            controller.push_error(ctx, GL_INVALID_ENUM);
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_fog_xv_emu, std::uint32_t target, const gl_fixed *param) {
        if ((target == GL_FOG_MODE_EMU) || (target == GL_FOG_START_EMU) || (target == GL_FOG_END_EMU) || (target == GL_FOG_DENSITY_EMU)) {
            gl_fog_x_emu(sys, target, *param);
            return;
        }
        
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target == GL_FOG_COLOR_EMU) {
            ctx->fog_color_[0] = FIXED_32_TO_FLOAT(param[0]);
            ctx->fog_color_[1] = FIXED_32_TO_FLOAT(param[1]);
            ctx->fog_color_[2] = FIXED_32_TO_FLOAT(param[2]);
            ctx->fog_color_[3] = FIXED_32_TO_FLOAT(param[3]);
        } else {
            controller.push_error(ctx, GL_INVALID_ENUM);
        }
    }

    static bool convert_gl_texenv_operand_to_our_enum(std::uint32_t value, gles_texture_env_info::source_operand &op, const bool is_for_alpha) {
        switch (value) {
        case GL_SRC_ALPHA_EMU:
            op = gles_texture_env_info::SOURCE_OPERAND_ALPHA;
            break;

        case GL_ONE_MINUS_SRC_ALPHA_EMU:
            op = gles_texture_env_info::SOURCE_OPERAND_ONE_MINUS_ALPHA;
            break;

        case GL_SRC_COLOR_EMU:
            if (is_for_alpha) {
                return false;
            }

            op = gles_texture_env_info::SOURCE_OPERAND_COLOR;
            break;

        case GL_ONE_MINUS_SRC_COLOR_EMU:
            if (is_for_alpha) {
                return false;
            }

            op = gles_texture_env_info::SOURCE_OPERAND_ONE_MINUS_COLOR;
            break;

        default:
            return false;
        }

        return true;
    }

    static bool convert_gl_texenv_src_to_our_enum(std::uint32_t value, gles_texture_env_info::source_type &tt) {
        switch (value) {
        case GL_TEXTURE_EMU:
            tt = gles_texture_env_info::SOURCE_TYPE_CURRENT_TEXTURE;
            break;

        case GL_CONSTANT_EMU:
            tt = gles_texture_env_info::SOURCE_TYPE_CONSTANT;
            break;

        case GL_PRIMARY_COLOR_EMU:
            tt = gles_texture_env_info::SOURCE_TYPE_PRIM_COLOR;
            break;

        case GL_PREVIOUS_EMU:
            tt = gles_texture_env_info::SOURCE_TYPE_PREVIOUS;
            break;

        case GL_TEXTURE0_EMU:
            tt = gles_texture_env_info::SOURCE_TYPE_TEXTURE_STAGE_0;
            break;

        case GL_TEXTURE1_EMU:
            tt = gles_texture_env_info::SOURCE_TYPE_TEXTURE_STAGE_1;
            break;

        case GL_TEXTURE2_EMU:
            tt = gles_texture_env_info::SOURCE_TYPE_TEXTURE_STAGE_2;
            break;

        default:
            return false;
        }

        return true;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_tex_envi_emu, std::uint32_t target, std::uint32_t name, std::int32_t param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_TEX_ENV_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        gles1_texture_unit &unit = ctx->texture_units_[ctx->active_texture_unit_];
        gles_texture_env_info &info = unit.env_info_;

        switch (name) {
        case GL_TEX_ENV_MODE_EMU:
            switch (param) {
            case GL_ADD_EMU:
                info.env_mode_ = gles_texture_env_info::ENV_MODE_ADD;
                break;

            case GL_BLEND_EMU:
                info.env_mode_ = gles_texture_env_info::ENV_MODE_BLEND;
                break;

            case GL_DECAL_EMU:
                info.env_mode_ = gles_texture_env_info::ENV_MODE_DECAL;
                break;

            case GL_MODULATE_EMU:
                info.env_mode_ = gles_texture_env_info::ENV_MODE_MODULATE;
                break;

            case GL_REPLACE_EMU:
                info.env_mode_ = gles_texture_env_info::ENV_MODE_REPLACE;
                break;

            case GL_COMBINE_EMU:
                info.env_mode_ = gles_texture_env_info::ENV_MODE_COMBINE;
                break;

            default:
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            break;

        case GL_COMBINE_RGB_EMU:
        case GL_COMBINE_ALPHA_EMU: {
            gles_texture_env_info::source_combine_function function;
            switch (param) {
            case GL_REPLACE_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_REPLACE;
                break;

            case GL_MODULATE_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_MODULATE;
                break;

            case GL_ADD_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_ADD;
                break;

            case GL_ADD_SIGNED_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_ADD_SIGNED;
                break;

            case GL_INTERPOLATE_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_INTERPOLATE;
                break;

            case GL_SUBTRACT_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_SUBTRACT;
                break;

            case GL_DOT3_RGB_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_DOT3_RGB;
                break;

            case GL_DOT3_RGBA_EMU:
                function = gles_texture_env_info::SOURCE_COMBINE_DOT3_RGBA;
                break;

            default:
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            if (name == GL_COMBINE_RGB_EMU) {
                info.combine_rgb_func_ = function;
            } else {
                info.combine_a_func_ = function;
            }

            break;
        }

        case GL_SRC0_ALPHA_EMU: {
            gles_texture_env_info::source_type type;
            if (!convert_gl_texenv_src_to_our_enum(param, type)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src0_a_ = type;
            break;
        }

        case GL_SRC0_RGB_EMU: {
            gles_texture_env_info::source_type type;
            if (!convert_gl_texenv_src_to_our_enum(param, type)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src0_rgb_ = type;
            break;
        }

        case GL_SRC1_ALPHA_EMU: {
            gles_texture_env_info::source_type type;
            if (!convert_gl_texenv_src_to_our_enum(param, type)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src1_a_ = type;
            break;
        }

        case GL_SRC1_RGB_EMU: {
            gles_texture_env_info::source_type type;
            if (!convert_gl_texenv_src_to_our_enum(param, type)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src1_rgb_ = type;
            break;
        }

        case GL_SRC2_ALPHA_EMU: {
            gles_texture_env_info::source_type type;
            if (!convert_gl_texenv_src_to_our_enum(param, type)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src2_a_ = type;
            break;
        }

        case GL_SRC2_RGB_EMU: {
            gles_texture_env_info::source_type type;
            if (!convert_gl_texenv_src_to_our_enum(param, type)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src2_rgb_ = type;
            break;
        }

        case GL_OPERAND0_RGB_EMU: {
            gles_texture_env_info::source_operand operand;
            if (!convert_gl_texenv_operand_to_our_enum(param, operand, false)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src0_rgb_op_ = operand;
            break;
        }

        case GL_OPERAND0_ALPHA_EMU: {
            gles_texture_env_info::source_operand operand;
            if (!convert_gl_texenv_operand_to_our_enum(param, operand, true)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src0_a_op_ = operand;
            break;
        }
        
        case GL_OPERAND1_RGB_EMU: {
            gles_texture_env_info::source_operand operand;
            if (!convert_gl_texenv_operand_to_our_enum(param, operand, false)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src1_rgb_op_ = operand;
            break;
        }

        case GL_OPERAND1_ALPHA_EMU: {
            gles_texture_env_info::source_operand operand;
            if (!convert_gl_texenv_operand_to_our_enum(param, operand, true)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src1_a_op_ = operand;
            break;
        }
        
        case GL_OPERAND2_RGB_EMU: {
            gles_texture_env_info::source_operand operand;
            if (!convert_gl_texenv_operand_to_our_enum(param, operand, false)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src2_rgb_op_ = operand;
            break;
        }

        case GL_OPERAND2_ALPHA_EMU: {
            gles_texture_env_info::source_operand operand;
            if (!convert_gl_texenv_operand_to_our_enum(param, operand, true)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            info.src2_a_op_ = operand;
            break;
        }

        case GL_RGB_SCALE_EMU:
            if ((param != 1) && (param != 2) && (param != 4)) {
                controller.push_error(ctx, GL_INVALID_VALUE);
                return;
            }

            unit.rgb_scale_ = static_cast<std::uint8_t>(param);
            break;

        case GL_ALPHA_SCALE_EMU:
            if ((param != 1) && (param != 2) && (param != 4)) {
                controller.push_error(ctx, GL_INVALID_VALUE);
                return;
            }

            unit.alpha_scale_ = static_cast<std::uint8_t>(param);
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_tex_envf_emu, std::uint32_t target, std::uint32_t name, float param) {
        gl_tex_envi_emu(sys, target, name, static_cast<std::int32_t>(param));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_envx_emu, std::uint32_t target, std::uint32_t name, gl_fixed param) {
        std::int32_t param_casted = static_cast<std::int32_t>(param);
        
        if ((name == GL_ALPHA_SCALE_EMU) || (name == GL_RGB_SCALE_EMU)) {
            param_casted = static_cast<std::int32_t>(FIXED_32_TO_FLOAT(param)); 
        }

        gl_tex_envi_emu(sys, target, name, param_casted);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_tex_enviv_emu, std::uint32_t target, std::uint32_t name, const std::int32_t *param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_TEX_ENV_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (name == GL_TEX_ENV_COLOR_EMU) {
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[0] = param[0] / 255.0f;
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[1] = param[1] / 255.0f;
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[2] = param[2] / 255.0f;
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[3] = param[3] / 255.0f;

            return;
        }

        gl_tex_envi_emu(sys, target, name, *param);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_envfv_emu, std::uint32_t target, std::uint32_t name, const float *param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_TEX_ENV_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (name == GL_TEX_ENV_COLOR_EMU) {
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[0] = param[0];
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[1] = param[1];
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[2] = param[2];
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[3] = param[3];

            return;
        }

        gl_tex_envi_emu(sys, target, name, static_cast<std::int32_t>(*param));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_envxv_emu, std::uint32_t target, std::uint32_t name, const gl_fixed *param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_TEX_ENV_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (name == GL_TEX_ENV_COLOR_EMU) {
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[0] = FIXED_32_TO_FLOAT(param[0]);
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[1] = FIXED_32_TO_FLOAT(param[1]);
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[2] = FIXED_32_TO_FLOAT(param[2]);
            ctx->texture_units_[ctx->active_texture_unit_].env_colors_[3] = FIXED_32_TO_FLOAT(param[3]);

            return;
        }

        std::int32_t param_casted = static_cast<std::int32_t>(*param);
        
        if ((name == GL_ALPHA_SCALE_EMU) || (name == GL_RGB_SCALE_EMU)) {
            param_casted = static_cast<std::int32_t>(FIXED_32_TO_FLOAT(*param)); 
        }

        gl_tex_envi_emu(sys, target, name, param_casted);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_get_pointerv_emu, std::uint32_t target, std::uint32_t *off) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (target) {
        case GL_VERTEX_ARRAY_POINTER_EMU:
            *off = ctx->vertex_attrib_.offset_;
            break;

        case GL_COLOR_ARRAY_POINTER_EMU:
            *off = ctx->color_attrib_.offset_;
            break;

        case GL_TEXTURE_COORD_ARRAY_POINTER_EMU:
            *off = ctx->texture_units_[ctx->active_texture_unit_].coord_attrib_.offset_;
            break;

        case GL_NORMAL_ARRAY_POINTER_EMU:
            *off = ctx->normal_attrib_.offset_;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_light_f_emu, std::uint32_t light, std::uint32_t pname, float param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((light < GL_LIGHT0_EMU) || (light >= GL_LIGHT0_EMU + GLES1_EMU_MAX_LIGHT)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        const std::uint32_t light_index = light - GL_LIGHT0_EMU;
        gles1_light_info &info = ctx->lights_[light_index];

        switch (pname) {
        case GL_SPOT_EXPONENT_EMU:
            info.spot_exponent_ = param;
            break;

        case GL_SPOT_CUTOFF_EMU:
            info.spot_cutoff_ = param;
            break;

        case GL_CONSTANT_ATTENUATION_EMU:
            info.attenuatation_[0] = param;
            break;

        case GL_LINEAR_ATTENUATION_EMU:
            info.attenuatation_[1] = param;
            break;

        case GL_QUADRATIC_ATTENUATION_EMU:
            info.attenuatation_[2] = param;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_light_x_emu, std::uint32_t light, std::uint32_t pname, gl_fixed param) {
        gl_light_f_emu(sys, light, pname, FIXED_32_TO_FLOAT(param));
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_light_fv_emu, std::uint32_t light, std::uint32_t pname, const float *param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((light < GL_LIGHT0_EMU) || (light >= GL_LIGHT0_EMU + GLES1_EMU_MAX_LIGHT)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        const std::uint32_t light_index = light - GL_LIGHT0_EMU;
        gles1_light_info &info = ctx->lights_[light_index];

        switch (pname) {
        case GL_SPOT_EXPONENT_EMU:
            info.spot_exponent_ = *param;
            break;

        case GL_SPOT_CUTOFF_EMU:
            info.spot_cutoff_ = *param;
            break;

        case GL_CONSTANT_ATTENUATION_EMU:
            info.attenuatation_[0] = *param;
            break;

        case GL_LINEAR_ATTENUATION_EMU:
            info.attenuatation_[1] = *param;
            break;

        case GL_QUADRATIC_ATTENUATION_EMU:
            info.attenuatation_[2] = *param;
            break;

        case GL_AMBIENT_EMU:
            std::memcpy(info.ambient_, param, 4 * sizeof(float));
            break;

        case GL_DIFFUSE_EMU:
            std::memcpy(info.diffuse_, param, 4 * sizeof(float));
            break;

        case GL_SPECULAR_EMU:
            std::memcpy(info.specular_, param, 4 * sizeof(float));
            break;

        case GL_POSITION_EMU: {
            std::memcpy(info.position_or_dir_, param, 4 * sizeof(float));
            glm::vec4 transformed_pos = ctx->model_view_mat_stack_.top() * glm::make_vec4(info.position_or_dir_);

            info.position_or_dir_transformed_[0] = transformed_pos.x;
            info.position_or_dir_transformed_[1] = transformed_pos.y;
            info.position_or_dir_transformed_[2] = transformed_pos.z;
            info.position_or_dir_transformed_[3] = info.position_or_dir_[3];
            
            break;
        }

        case GL_SPOT_DIRECTION_EMU: {
            std::memcpy(info.spot_dir_, param, 3 * sizeof(float));

            glm::vec3 transformed_spot_dir = (glm::mat3(ctx->model_view_mat_stack_.top()) * glm::make_vec3(info.spot_dir_)) * glm::vec3(-1);

            info.spot_dir_transformed_[0] = transformed_spot_dir.x;
            info.spot_dir_transformed_[1] = transformed_spot_dir.y;
            info.spot_dir_transformed_[2] = transformed_spot_dir.z;
            
            break;
        }

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_light_xv_emu, std::uint32_t light, std::uint32_t pname, const gl_fixed *param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((light < GL_LIGHT0_EMU) || (light >= GL_LIGHT0_EMU + GLES1_EMU_MAX_LIGHT)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        const std::uint32_t light_index = light - GL_LIGHT0_EMU;
        gles1_light_info &info = ctx->lights_[light_index];

        switch (pname) {
        case GL_SPOT_EXPONENT_EMU:
            info.spot_exponent_ = FIXED_32_TO_FLOAT(*param);
            break;

        case GL_SPOT_CUTOFF_EMU:
            info.spot_cutoff_ = FIXED_32_TO_FLOAT(*param);
            break;

        case GL_CONSTANT_ATTENUATION_EMU:
            info.attenuatation_[0] = FIXED_32_TO_FLOAT(*param);
            break;

        case GL_LINEAR_ATTENUATION_EMU:
            info.attenuatation_[1] = FIXED_32_TO_FLOAT(*param);
            break;

        case GL_QUADRATIC_ATTENUATION_EMU:
            info.attenuatation_[2] = FIXED_32_TO_FLOAT(*param);
            break;

        case GL_AMBIENT_EMU:
            info.ambient_[0] = FIXED_32_TO_FLOAT(param[0]);
            info.ambient_[1] = FIXED_32_TO_FLOAT(param[1]);
            info.ambient_[2] = FIXED_32_TO_FLOAT(param[2]);
            info.ambient_[3] = FIXED_32_TO_FLOAT(param[3]);
            break;

        case GL_DIFFUSE_EMU:
            info.diffuse_[0] = FIXED_32_TO_FLOAT(param[0]);
            info.diffuse_[1] = FIXED_32_TO_FLOAT(param[1]);
            info.diffuse_[2] = FIXED_32_TO_FLOAT(param[2]);
            info.diffuse_[3] = FIXED_32_TO_FLOAT(param[3]);
            break;

        case GL_SPECULAR_EMU:
            info.specular_[0] = FIXED_32_TO_FLOAT(param[0]);
            info.specular_[1] = FIXED_32_TO_FLOAT(param[1]);
            info.specular_[2] = FIXED_32_TO_FLOAT(param[2]);
            info.specular_[3] = FIXED_32_TO_FLOAT(param[3]);
            break;

        case GL_POSITION_EMU: {
            info.position_or_dir_[0] = FIXED_32_TO_FLOAT(param[0]);
            info.position_or_dir_[1] = FIXED_32_TO_FLOAT(param[1]);
            info.position_or_dir_[2] = FIXED_32_TO_FLOAT(param[2]);
            info.position_or_dir_[3] = FIXED_32_TO_FLOAT(param[3]);
            
            glm::vec4 transformed_pos = ctx->model_view_mat_stack_.top() * glm::make_vec4(info.position_or_dir_);

            info.position_or_dir_transformed_[0] = transformed_pos.x;
            info.position_or_dir_transformed_[1] = transformed_pos.y;
            info.position_or_dir_transformed_[2] = transformed_pos.z;
            info.position_or_dir_transformed_[3] = info.position_or_dir_[3];
            
            break;
        }

        case GL_SPOT_DIRECTION_EMU: {
            info.spot_dir_[0] = FIXED_32_TO_FLOAT(param[0]);
            info.spot_dir_[1] = FIXED_32_TO_FLOAT(param[1]);
            info.spot_dir_[2] = FIXED_32_TO_FLOAT(param[2]);
            
            glm::vec3 transformed_spot_dir = (glm::mat3(ctx->model_view_mat_stack_.top()) * glm::make_vec3(info.spot_dir_)) * glm::vec3(-1);

            info.spot_dir_transformed_[0] = transformed_spot_dir.x;
            info.spot_dir_transformed_[1] = transformed_spot_dir.y;
            info.spot_dir_transformed_[2] = transformed_spot_dir.z;
            
            break;
        }

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_light_model_f_emu, std::uint32_t pname, float param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (pname != GL_LIGHT_MODEL_TWO_SIDE_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (param != 0.0f) {
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT_TWO_SIDE;
        } else {
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT_TWO_SIDE;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_light_model_x_emu, std::uint32_t pname, gl_fixed param) {
        gl_light_model_f_emu(sys, pname, FIXED_32_TO_FLOAT(param));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_light_model_fv_emu, std::uint32_t pname, float *param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (pname) {
        case GL_LIGHT_MODEL_AMBIENT_EMU:
            ctx->global_ambient_[0] = param[0];
            ctx->global_ambient_[1] = param[1];
            ctx->global_ambient_[2] = param[2];
            ctx->global_ambient_[3] = param[3];
            break;

        case GL_LIGHT_MODEL_TWO_SIDE_EMU:
            if (*param != 0.0f) {
                ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT_TWO_SIDE;
            } else {
                ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT_TWO_SIDE;
            }
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_light_model_xv_emu, std::uint32_t pname, gl_fixed *param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (pname) {
        case GL_LIGHT_MODEL_AMBIENT_EMU:
            ctx->global_ambient_[0] = FIXED_32_TO_FLOAT(param[0]);
            ctx->global_ambient_[1] = FIXED_32_TO_FLOAT(param[1]);
            ctx->global_ambient_[2] = FIXED_32_TO_FLOAT(param[2]);
            ctx->global_ambient_[3] = FIXED_32_TO_FLOAT(param[3]);
            break;

        case GL_LIGHT_MODEL_TWO_SIDE_EMU:
            if (*param != 0) {
                ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT_TWO_SIDE;
            } else {
                ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT_TWO_SIDE;
            }
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_clip_plane_f_emu, std::uint32_t plane, float *eq) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((plane < GL_CLIP_PLANE0_EMU) && (plane > GL_CLIP_PLANE5_EMU)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        std::memcpy(ctx->clip_planes_[plane - GL_CLIP_PLANE0_EMU], eq, 4 * sizeof(float));

        glm::vec4 transformed = glm::inverse(glm::transpose(ctx->model_view_mat_stack_.top())) * glm::make_vec4(ctx->clip_planes_[plane - GL_CLIP_PLANE0_EMU]);
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][0] = transformed.x;
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][1] = transformed.y;
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][2] = transformed.z;
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][3] = transformed.w;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_clip_plane_x_emu, std::uint32_t plane, gl_fixed *eq) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((plane < GL_CLIP_PLANE0_EMU) && (plane > GL_CLIP_PLANE5_EMU)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        const std::uint32_t plane_index = plane - GL_CLIP_PLANE0_EMU;

        ctx->clip_planes_[plane_index][0] = FIXED_32_TO_FLOAT(eq[0]);
        ctx->clip_planes_[plane_index][1] = FIXED_32_TO_FLOAT(eq[1]);
        ctx->clip_planes_[plane_index][2] = FIXED_32_TO_FLOAT(eq[2]);
        ctx->clip_planes_[plane_index][3] = FIXED_32_TO_FLOAT(eq[3]); 

        glm::vec4 transformed = glm::make_vec4(ctx->clip_planes_[plane - GL_CLIP_PLANE0_EMU]) * glm::inverse(ctx->model_view_mat_stack_.top());
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][0] = transformed.x;
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][1] = transformed.y;
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][2] = transformed.z;
        ctx->clip_planes_transformed_[plane - GL_CLIP_PLANE0_EMU][3] = transformed.w;
    }

    bool egl_context_es1::prepare_shader_program_for_draw(dispatch::egl_controller &controller, const std::uint32_t active_texs) {
        dispatch::gles_texture_env_info info_arr[GLES1_EMU_MAX_TEXTURE_COUNT];
        for (std::int32_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
            info_arr[i] = texture_units_[i].env_info_;
        }

        dispatch::gles1_shader_variables_info *var_info = nullptr;

        drivers::handle program = controller.get_es1_shaderman().retrieve_program(vertex_statuses_, fragment_statuses_, active_texs,
            info_arr, var_info);

        if (!program) {
            LOG_ERROR(HLE_DISPATCHER, "Problem while retrieveing GLES1 shader program!");
            return false;
        }

        cmd_builder_.use_program(program);

        if (var_info) {
            // Not binded by uniform buffer/constant buffer
            if (vertex_statuses_ & egl_context_es1::VERTEX_STATE_SKINNING_ENABLE) {
                std::vector<std::uint8_t> palette_mats_data(64 * GLES1_EMU_MAX_PALETTE_MATRICES);
                for (std::size_t i = 0; i < GLES1_EMU_MAX_PALETTE_MATRICES; i++) {
                    memcpy(palette_mats_data.data() + i * 64, glm::value_ptr(palette_mats_[i]), 64);
                }
                cmd_builder_.set_dynamic_uniform(var_info->palette_mat_loc_, drivers::shader_var_type::mat4,
                    palette_mats_data.data(), palette_mats_data.size());
            } else {
                cmd_builder_.set_dynamic_uniform(var_info->view_model_mat_loc_, drivers::shader_var_type::mat4,
                    glm::value_ptr(model_view_mat_stack_.top()), 64);
            }

            cmd_builder_.set_dynamic_uniform(var_info->proj_mat_loc_, drivers::shader_var_type::mat4,
                glm::value_ptr(proj_mat_stack_.top()), 64);

            if ((vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY) == 0) {
                cmd_builder_.set_dynamic_uniform(var_info->color_loc_, drivers::shader_var_type::vec4,
                    color_uniforms_, 16);
            }

            if ((vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY) == 0) {
                cmd_builder_.set_dynamic_uniform(var_info->normal_loc_, drivers::shader_var_type::vec3,
                    normal_uniforms_, 12);
            }

            std::uint64_t coordarray_mask = egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD0_ARRAY;

            for (std::uint32_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++, coordarray_mask <<= 1) {
                if (active_texs & (1 << i)) {
                    if ((vertex_statuses_ & coordarray_mask) == 0)
                        cmd_builder_.set_dynamic_uniform(var_info->texcoord_loc_[i], drivers::shader_var_type::vec4,
                            texture_units_[i].coord_uniforms_, 16);

                    cmd_builder_.set_dynamic_uniform(var_info->texture_mat_loc_[i], drivers::shader_var_type::mat4,
                        glm::value_ptr(texture_units_[i].texture_mat_stack_.top()), 64);

                    cmd_builder_.set_texture_for_shader(i, var_info->texview_loc_[i], drivers::shader_module_type::fragment);
                    cmd_builder_.set_dynamic_uniform(var_info->texenv_color_loc_[i], drivers::shader_var_type::vec4,
                        texture_units_[i].env_colors_, 16);
                }
            }
            
            for (std::uint8_t i = 0; i < GLES1_EMU_MAX_CLIP_PLANE; i++) {
                if (fragment_statuses_ & (1 << (egl_context_es1::FRAGMENT_STATE_CLIP_PLANE_BIT_POS + i))) {
                    cmd_builder_.set_dynamic_uniform(var_info->clip_plane_loc_[i], drivers::shader_var_type::vec4,
                        clip_planes_transformed_[i], 16);
                }
            }

            cmd_builder_.set_dynamic_uniform(var_info->material_ambient_loc_, drivers::shader_var_type::vec4,
                material_ambient_, 16);

            cmd_builder_.set_dynamic_uniform(var_info->material_diffuse_loc_, drivers::shader_var_type::vec4,
                material_diffuse_, 16);
            
            cmd_builder_.set_dynamic_uniform(var_info->material_specular_loc_, drivers::shader_var_type::vec4,
                material_specular_, 16);

            cmd_builder_.set_dynamic_uniform(var_info->material_emission_loc_, drivers::shader_var_type::vec4,
                material_emission_, 16);

            cmd_builder_.set_dynamic_uniform(var_info->material_shininess_loc_, drivers::shader_var_type::real,
                &material_shininess_, 4);

            cmd_builder_.set_dynamic_uniform(var_info->global_ambient_loc_, drivers::shader_var_type::vec4,
                global_ambient_, 16);

            if (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_ALPHA_TEST) {
                cmd_builder_.set_dynamic_uniform(var_info->alpha_test_ref_loc_, drivers::shader_var_type::real,
                    &alpha_test_ref_, 4);
            }

            if (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_FOG_ENABLE) {
                cmd_builder_.set_dynamic_uniform(var_info->fog_color_loc_, drivers::shader_var_type::vec4,
                    fog_color_, 16);

                std::uint64_t fog_mode = (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_FOG_MODE_MASK);

                if (fog_mode == egl_context_es1::FRAGMENT_STATE_FOG_MODE_LINEAR) {
                    cmd_builder_.set_dynamic_uniform(var_info->fog_start_loc_, drivers::shader_var_type::real,
                        &fog_start_, 4);
                    cmd_builder_.set_dynamic_uniform(var_info->fog_end_loc_, drivers::shader_var_type::real,
                        &fog_end_, 4);
                } else {
                    cmd_builder_.set_dynamic_uniform(var_info->fog_density_loc_, drivers::shader_var_type::real,
                        &fog_density_, 4);
                }
            }

            if (fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHTING_ENABLE) {
                for (std::uint32_t i = 0, mask = egl_context_es1::FRAGMENT_STATE_LIGHT0_ON; i < GLES1_EMU_MAX_LIGHT; i++, mask <<= 1) {
                    if (fragment_statuses_ & mask) {
                        cmd_builder_.set_dynamic_uniform(var_info->light_dir_or_pos_loc_[i], drivers::shader_var_type::vec4,
                            lights_[i].position_or_dir_transformed_, 16);
                        cmd_builder_.set_dynamic_uniform(var_info->light_ambient_loc_[i], drivers::shader_var_type::vec4,
                            lights_[i].ambient_, 16);
                        cmd_builder_.set_dynamic_uniform(var_info->light_diffuse_loc_[i], drivers::shader_var_type::vec4,
                            lights_[i].diffuse_, 16);
                        cmd_builder_.set_dynamic_uniform(var_info->light_specular_loc_[i], drivers::shader_var_type::vec4,
                            lights_[i].specular_, 16);
                        cmd_builder_.set_dynamic_uniform(var_info->light_spot_dir_loc_[i], drivers::shader_var_type::vec3,
                            lights_[i].spot_dir_transformed_, 12);
                        cmd_builder_.set_dynamic_uniform(var_info->light_spot_cutoff_loc_[i], drivers::shader_var_type::real,
                            &lights_[i].spot_cutoff_, 4);
                        cmd_builder_.set_dynamic_uniform(var_info->light_spot_exponent_loc_[i], drivers::shader_var_type::real,
                            &lights_[i].spot_exponent_, 4);
                        cmd_builder_.set_dynamic_uniform(var_info->light_attenuatation_vec_loc_[i], drivers::shader_var_type::vec3,
                            lights_[i].attenuatation_, 12);
                    }
                }
            }
        } else {
            LOG_WARN(HLE_DISPATCHER, "Shader variables in buffer not yet supported!");
        }

        return true;
    }

    void egl_context_es1::prepare_vertex_buffer_and_descriptors(drivers::graphics_driver *drv, kernel::process *crr_process, const std::int32_t first_index, const std::uint32_t vcount, const std::uint32_t active_texs) {
        if (attrib_changed_ || (previous_first_index_ != first_index)) {
            std::vector<drivers::handle> vertex_buffers_alloc;
            bool not_persistent = false;

            auto retrieve_vertex_buffer_slot = [&](gles_vertex_attrib attrib, std::uint32_t &res, int &offset) -> bool {
                return this->retrieve_vertex_buffer_slot(vertex_buffers_alloc, drv, crr_process, attrib, first_index, vcount, res, offset, not_persistent);
            };

            // Remade and attach descriptors
            std::vector<drivers::input_descriptor> descs;
            drivers::data_format temp_format;

            drivers::input_descriptor temp_desc;
            if (!retrieve_vertex_buffer_slot(vertex_attrib_, temp_desc.buffer_slot, temp_desc.offset)) {
                LOG_WARN(HLE_DISPATCHER, "Vertex attribute not bound to a valid buffer, draw call skipping!");
                return;
            }

            temp_desc.location = 0;
            temp_desc.stride = vertex_attrib_.stride_;

            gl_enum_to_drivers_data_format(vertex_attrib_.data_type_, temp_format);
            temp_desc.set_format(vertex_attrib_.size_, temp_format);

            if (vertex_attrib_.data_type_ == GL_FIXED_EMU) {
                temp_desc.set_normalized(true);
            }

            descs.push_back(temp_desc);

            if (vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY) {
                if (retrieve_vertex_buffer_slot(color_attrib_, temp_desc.buffer_slot, temp_desc.offset)) {
                    temp_desc.location = 1;
                    temp_desc.stride = color_attrib_.stride_;
                    temp_desc.set_normalized(true);

                    gl_enum_to_drivers_data_format(color_attrib_.data_type_, temp_format);
                    temp_desc.set_format(color_attrib_.size_, temp_format);

                    descs.push_back(temp_desc);
                }
            }

            temp_desc.set_normalized(false);

            if (vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY) {
                if (retrieve_vertex_buffer_slot(normal_attrib_, temp_desc.buffer_slot, temp_desc.offset)) {
                    temp_desc.location = 2;
                    temp_desc.stride = normal_attrib_.stride_;

                    gl_enum_to_drivers_data_format(normal_attrib_.data_type_, temp_format);
                    temp_desc.set_format(normal_attrib_.size_, temp_format);
                    temp_desc.set_normalized(true);

                    descs.push_back(temp_desc);
                }
            }

            temp_desc.set_normalized(false);

            if (active_texs) {
                for (std::int32_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
                    if ((active_texs & (1 << i)) && (vertex_statuses_ & (1 << (egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD_ARRAY_POS + static_cast<std::uint8_t>(i))))
                        && retrieve_vertex_buffer_slot(texture_units_[i].coord_attrib_, temp_desc.buffer_slot, temp_desc.offset)) {
                        temp_desc.location = 3 + i;
                        temp_desc.stride = texture_units_[i].coord_attrib_.stride_;

                        gl_enum_to_drivers_data_format(texture_units_[i].coord_attrib_.data_type_, temp_format);
                        temp_desc.set_format(texture_units_[i].coord_attrib_.size_, temp_format);

                        descs.push_back(temp_desc);
                    }
                }
            }

            temp_desc.set_normalized(false);

            if (vertex_statuses_ & egl_context_es1::VERTEX_STATE_SKINNING_ENABLE) {
                if (vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_MATRIX_INDEX_ARRAY) {
                    if (retrieve_vertex_buffer_slot(matrix_index_attrib_, temp_desc.buffer_slot, temp_desc.offset)) {
                        temp_desc.location = 6;
                        temp_desc.stride = matrix_index_attrib_.stride_;
                        temp_desc.set_format(matrix_index_attrib_.size_, drivers::data_format::byte);

                        descs.push_back(temp_desc);
                    }
                }

                if (vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_WEIGHT_ARRAY) {
                    if (retrieve_vertex_buffer_slot(weight_attrib_, temp_desc.buffer_slot, temp_desc.offset)) {
                        temp_desc.location = 7;
                        temp_desc.stride = weight_attrib_.stride_;

                        gl_enum_to_drivers_data_format(weight_attrib_.data_type_, temp_format);
                        temp_desc.set_format(weight_attrib_.size_, temp_format);

                        if (temp_format == drivers::data_format::fixed) {
                            temp_desc.set_normalized(true);
                        }

                        descs.push_back(temp_desc);
                    }
                }
            }

            if (!input_desc_) {
                input_desc_ = drivers::create_input_descriptors(drv, descs.data(), static_cast<std::uint32_t>(descs.size()));
            } else {
                cmd_builder_.update_input_descriptors(input_desc_, descs.data(), static_cast<std::uint32_t>(descs.size()));
            }

            cmd_builder_.set_vertex_buffers(vertex_buffers_alloc.data(), 0, static_cast<std::uint32_t>(vertex_buffers_alloc.size()));

            if (!not_persistent)
                attrib_changed_ = false;

            previous_first_index_ = first_index;
        }

        cmd_builder_.bind_input_descriptors(input_desc_);
    }

    static std::uint32_t retrieve_active_textures_bitarr(egl_context_es1 *ctx) {
        std::uint32_t arr = 0;
        for (std::size_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
            if (!ctx->texture_units_[i].texturing_enabled_) {
                continue;
            }
            auto *inst = ctx->objects_.get(ctx->texture_units_[i].binded_texture_handle_);
            if (inst && (*inst).get()) {
                arr |= (1 << i);
            }
        }

        return arr;
    }

    bool egl_context_es1::prepare_for_draw(drivers::graphics_driver *drv, egl_controller &controller, kernel::process *crr_process,
        const std::int32_t first_index, const std::uint32_t vcount)  {
        if ((vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_VERTEX_ARRAY) == 0) {
            // No drawing needed?
            return true;
        }

        std::uint32_t active_textures_bitarr = retrieve_active_textures_bitarr(this);
        prepare_vertex_buffer_and_descriptors(drv, crr_process, first_index, vcount, active_textures_bitarr);

        flush_state_changes();

        // Active textures
        for (std::int32_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
            if (active_textures_bitarr & (1 << i)) {
                auto *obj = objects_.get(texture_units_[i].binded_texture_handle_);

                if (obj)
                    cmd_builder_.bind_texture((*obj)->handle_value(), i);
            }
        }

        if (!prepare_shader_program_for_draw(controller, active_textures_bitarr)) {
            return false;
        }

        return true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_hint_emu) {
        // Empty intentionally.
    }

    BRIDGE_FUNC_LIBRARY(void, gl_current_palette_matrix_oes_emu, std::uint32_t index) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (ctx->active_mat_stack_ != GL_MATRIX_PALETTE_OES) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        if (index >= GLES1_EMU_MAX_PALETTE_MATRICES) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        ctx->current_palette_mat_index_ = index;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_load_palette_from_model_view_matrix_oes_emu) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (ctx->active_mat_stack_ != GL_MATRIX_PALETTE_OES) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        ctx->palette_mats_[ctx->current_palette_mat_index_] = ctx->model_view_mat_stack_.top();
    }

    BRIDGE_FUNC_LIBRARY(void, gl_weight_pointer_oes_emu, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((stride < 0) || ((type != GL_FIXED_EMU) && (type != GL_FLOAT_EMU)) || (size == 0) || (size > GLES1_EMU_MAX_WEIGHTS_PER_VERTEX)) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (!assign_vertex_attrib_gles(ctx->weight_attrib_, size, type, stride, offset, ctx->binded_array_buffer_handle_)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->attrib_changed_ = true;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_matrix_index_pointer_oes_emu, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((stride < 0) || (type != GL_UNSIGNED_BYTE_EMU) || (size == 0) || (size > GLES1_EMU_MAX_WEIGHTS_PER_VERTEX)) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (!assign_vertex_attrib_gles(ctx->matrix_index_attrib_, size, type, stride, offset, ctx->binded_array_buffer_handle_)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        // According to the documentation: the number of N is indicated by value passed to glMatrixIndexPointerOES
        // The whole extension document is not too strict.
        ctx->vertex_statuses_ |= ((size & 0b11) << egl_context_es1::VERTEX_STATE_SKIN_WEIGHTS_PER_VERTEX_BITS_POS);
        ctx->attrib_changed_ = true;
    }
}