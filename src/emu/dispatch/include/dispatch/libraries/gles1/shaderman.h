/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <cstdint>
#include <unordered_map>
#include <string>

#include <drivers/graphics/common.h>
#include <drivers/graphics/graphics.h>
#include <dispatch/libraries/gles1/consts.h>

namespace eka2l1::dispatch {
    struct gles_texture_env_info;

    struct gles1_shader_variables_info {
        std::int32_t view_model_mat_loc_;
        std::int32_t proj_mat_loc_;
        std::int32_t color_loc_;
        std::int32_t normal_loc_;
        std::int32_t palette_mat_loc_;
        std::int32_t texcoord_loc_[GLES1_EMU_MAX_TEXTURE_COUNT];
        std::int32_t texview_loc_[GLES1_EMU_MAX_TEXTURE_COUNT];
        std::int32_t texenv_color_loc_[GLES1_EMU_MAX_TEXTURE_COUNT];
        std::int32_t texture_mat_loc_[GLES1_EMU_MAX_TEXTURE_COUNT];
        std::int32_t clip_plane_loc_[GLES1_EMU_MAX_CLIP_PLANE];
        std::int32_t material_ambient_loc_;
        std::int32_t material_diffuse_loc_;
        std::int32_t material_specular_loc_;
        std::int32_t material_emission_loc_;
        std::int32_t material_shininess_loc_;
        std::int32_t global_ambient_loc_;
        std::int32_t alpha_test_ref_loc_;
        std::int32_t fog_color_loc_;
        std::int32_t fog_density_loc_;
        std::int32_t fog_start_loc_;
        std::int32_t fog_end_loc_;
        std::int32_t light_dir_or_pos_loc_[GLES1_EMU_MAX_LIGHT];
        std::int32_t light_ambient_loc_[GLES1_EMU_MAX_LIGHT];
        std::int32_t light_diffuse_loc_[GLES1_EMU_MAX_LIGHT];
        std::int32_t light_specular_loc_[GLES1_EMU_MAX_LIGHT];
        std::int32_t light_spot_dir_loc_[GLES1_EMU_MAX_LIGHT];
        std::int32_t light_spot_cutoff_loc_[GLES1_EMU_MAX_LIGHT];
        std::int32_t light_spot_exponent_loc_[GLES1_EMU_MAX_LIGHT];
        std::int32_t light_attenuatation_vec_loc_[GLES1_EMU_MAX_LIGHT];
    };

    struct gles1_shaderman {
    protected:
        std::unordered_map<std::uint64_t, drivers::handle> vertex_cache_;
        std::unordered_map<std::uint64_t, drivers::handle> fragment_cache_;
        std::unordered_map<std::uint64_t, std::unordered_map<std::uint64_t,
            std::pair<drivers::handle, std::unique_ptr<gles1_shader_variables_info>>>> program_cache_;

        drivers::graphics_driver *driver_;
        void *fragment_status_hasher_;

    public:
        explicit gles1_shaderman(drivers::graphics_driver *driver);
        ~gles1_shaderman();
        
        void set_graphics_driver(drivers::graphics_driver *driver);

        drivers::handle retrieve_program(const std::uint64_t vertex_statuses, const std::uint64_t fragment_statuses,
            const std::uint32_t active_texs, gles_texture_env_info *tex_env_infos, gles1_shader_variables_info *&info);
    };
}