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

#include <dispatch/libraries/gles1/shaderman.h>
#include <dispatch/libraries/gles1/shadergen.h>

#include <dispatch/libraries/gles1/def.h>

#define XXH_INLINE_ALL
#include <xxhash.h>

namespace eka2l1::dispatch {
    gles1_shaderman::gles1_shaderman(drivers::graphics_driver *driver)
        : driver_(driver)
        , fragment_status_hasher_(nullptr) {

    }

    void gles1_shaderman::set_graphics_driver(drivers::graphics_driver *driver) {
        driver_ = driver;
    }

    gles1_shaderman::~gles1_shaderman() {
        if (driver_) {
            drivers::graphics_command_builder builder;

            for (auto &module: vertex_cache_) {
                builder.destroy(module.second);
            }

            for (auto &module: fragment_cache_) {
                builder.destroy(module.second);
            }

            for (auto &vert_index: program_cache_) {
                for (auto &frag_index: vert_index.second)
                    builder.destroy(frag_index.second.first);
            }

            drivers::command_list retrieved = builder.retrieve_command_list();
            driver_->submit_command_list(retrieved);
        }

        if (fragment_status_hasher_) {
            XXH64_freeState(reinterpret_cast<XXH64_state_t*>(fragment_status_hasher_));
        }
    }
    
    drivers::handle gles1_shaderman::retrieve_program(const std::uint64_t vertex_statuses, const std::uint64_t fragment_statuses,
        const std::uint32_t active_texs, gles_texture_env_info *tex_env_infos, gles1_shader_variables_info *&info) {
        // Turn off states that are not used (for hashing)
        std::uint64_t cleansed_fragment_statuses = fragment_statuses;
        if ((fragment_statuses & egl_context_es1::FRAGMENT_STATE_ALPHA_TEST) == 0) {
            cleansed_fragment_statuses &= ~egl_context_es1::FRAGMENT_STATE_ALPHA_FUNC_MASK;
        }

        if ((fragment_statuses & egl_context_es1::FRAGMENT_STATE_FOG_ENABLE) == 0) {
            cleansed_fragment_statuses &= ~egl_context_es1::FRAGMENT_STATE_FOG_MODE_MASK;
        }

        if ((fragment_statuses & egl_context_es1::FRAGMENT_STATE_LIGHTING_ENABLE) == 0) {
            cleansed_fragment_statuses &= ~egl_context_es1::FRAGMENT_STATE_LIGHT_RELATED_MASK;
        }

        drivers::handle vert_module = 0;
        std::uint64_t vertex_hash = vertex_statuses | (static_cast<std::uint64_t>(active_texs) << egl_context_es1::VERTEX_STATE_REVERSED_BITS_POS);
        
        // These are only used for state tracking really!
        vertex_hash &= ~(egl_context_es1::VERTEX_STATE_CLIENT_WEIGHT_ARRAY | egl_context_es1::VERTEX_STATE_CLIENT_MATRIX_INDEX_ARRAY);

        if ((vertex_hash & egl_context_es1::VERTEX_STATE_SKINNING_ENABLE) == 0) {
            vertex_hash &= ~egl_context_es1::VERTEX_STATE_SKIN_WEIGHTS_PER_VERTEX_MASK;
        }

        if (active_texs != 0) {
            // Clean texcoord bits of unused textures...
            for (std::uint8_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
                if ((active_texs & (1 << i)) == 0) {
                    vertex_hash &= ~(1 << (egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD_ARRAY_POS + i));
                }
            }
        }

        auto vert_cache_ite = vertex_cache_.find(vertex_hash);
        if (vert_cache_ite == vertex_cache_.end()) {
            std::string source_shader;
            switch (driver_->get_current_api()) {
            case drivers::graphic_api::opengl:
                source_shader = generate_gl_vertex_shader(vertex_statuses, active_texs, driver_->is_stricted());
                break;

            default:
                LOG_ERROR(HLE_DISPATCHER, "Current backend does not support GLES1 shadergen yet!");
                return 0;
            }

            vert_module = drivers::create_shader_module(driver_, source_shader.data(), source_shader.size(),
                drivers::shader_module_type::vertex);

            if (!vert_module) {
                LOG_ERROR(HLE_DISPATCHER, "Fail to create GLES1 vertex shader module!");
                return 0;
            }

            vertex_cache_.emplace(vertex_hash, vert_module);
        } else {
            vert_module = vert_cache_ite->second;
        }

        drivers::handle fragment_module = 0;
        if (!fragment_status_hasher_) {
            fragment_status_hasher_ = XXH64_createState();
        }

        // Doodle GLES1 (the seed I try to write 0_0)
        XXH64_reset(reinterpret_cast<XXH64_state_t*>(fragment_status_hasher_), 0xD00D1E61E51ULL);
        XXH64_update(reinterpret_cast<XXH64_state_t*>(fragment_status_hasher_), &cleansed_fragment_statuses, sizeof(std::uint64_t));

        if (active_texs != 0) {
            for (std::size_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
                if (active_texs & (1 << i)) {
                    XXH64_update(reinterpret_cast<XXH64_state_t*>(fragment_status_hasher_), tex_env_infos + i, sizeof(gles_texture_env_info));
                }
            }
        }

        std::uint64_t fragment_module_hash = XXH64_digest(reinterpret_cast<XXH64_state_t*>(fragment_status_hasher_));
        auto frag_cache_ite = fragment_cache_.find(fragment_module_hash);
        if (frag_cache_ite == fragment_cache_.end()) {
            std::string source_shader;
            switch (driver_->get_current_api()) {
            case drivers::graphic_api::opengl:
                source_shader = generate_gl_fragment_shader(cleansed_fragment_statuses, active_texs, tex_env_infos, driver_->is_stricted());
                break;

            default:
                LOG_ERROR(HLE_DISPATCHER, "Current backend does not support GLES1 shadergen yet!");
                return 0;
            }

            fragment_module = drivers::create_shader_module(driver_, source_shader.data(), source_shader.size(),
                drivers::shader_module_type::fragment);

            if (!fragment_module) {
                LOG_ERROR(HLE_DISPATCHER, "Fail to create GLES1 fragment shader module!");
                return 0;
            }

            fragment_cache_.emplace(fragment_module_hash, fragment_module);
        } else {
            fragment_module = frag_cache_ite->second;
        }

        // Got the two handles, try to produce a program
        bool need_produce_program = false;

        auto level1_program_ite = program_cache_.find(vert_module);
        if (level1_program_ite == program_cache_.end()) {
            need_produce_program = true;
        } else {
            auto level2_program_ite = level1_program_ite->second.find(fragment_module);
            if (level2_program_ite == level1_program_ite->second.end()) {
                need_produce_program = true;
            } else {
                info = level2_program_ite->second.second.get();
                return level2_program_ite->second.first;
            }
        }

        drivers::shader_program_metadata metadata(nullptr);
        drivers::handle program_handle = drivers::create_shader_program(driver_, vert_module, fragment_module, &metadata);
        if (!program_handle) {
            LOG_ERROR(HLE_DISPATCHER, "Fail to create GLES1 shader program!");
            return 0;
        }

        std::unique_ptr<gles1_shader_variables_info> info_inst = nullptr;
        if (metadata.is_available()) {
            info_inst = std::make_unique<gles1_shader_variables_info>();

            if ((vertex_statuses & egl_context_es1::VERTEX_STATE_SKINNING_ENABLE) == 0) {
                info_inst->view_model_mat_loc_ = metadata.get_uniform_binding("uViewModelMat");
            } else {
                info_inst->palette_mat_loc_ = metadata.get_uniform_binding("uPaletteMat");
            }

            info_inst->proj_mat_loc_ = metadata.get_uniform_binding("uProjMat");

            if ((vertex_statuses & egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY) == 0)
                info_inst->color_loc_ = metadata.get_uniform_binding("uColor");

            if ((vertex_statuses & egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY) == 0)
                info_inst->normal_loc_ = metadata.get_uniform_binding("uNormal");

            std::string texcoordname = "uTexCoord0";
            std::string texviewname = "uTexture0";
            std::string texenv_color_name = "uTextureEnvColor0";
            std::string texture_mat_name = "uTextureMat0";
            std::string clip_plane_name = "uClipPlane0";

            for (std::uint32_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
                if (active_texs & (1 << i)) {
                    if ((vertex_statuses & (1 << (static_cast<std::uint8_t>(i) + egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD_ARRAY_POS))) == 0)
                        info_inst->texcoord_loc_[i] = metadata.get_uniform_binding(texcoordname.c_str());
        
                    info_inst->texture_mat_loc_[i] = metadata.get_uniform_binding(texture_mat_name.c_str());
                    info_inst->texview_loc_[i] = metadata.get_uniform_binding(texviewname.c_str());
                    info_inst->texenv_color_loc_[i] = metadata.get_uniform_binding(texenv_color_name.c_str());
                }

                texcoordname.back()++;
                texviewname.back()++;
                texture_mat_name.back()++;
                texenv_color_name.back()++;
            }

            for (std::uint8_t i = 0; i < GLES1_EMU_MAX_CLIP_PLANE; i++) {
                if (fragment_statuses & (1 << (egl_context_es1::FRAGMENT_STATE_CLIP_PLANE_BIT_POS + i))) {
                    info_inst->clip_plane_loc_[i] = metadata.get_uniform_binding(clip_plane_name.c_str());
                }

                clip_plane_name.back()++;
            }

            info_inst->material_ambient_loc_ = metadata.get_uniform_binding("uMaterialAmbient");
            info_inst->material_diffuse_loc_ = metadata.get_uniform_binding("uMaterialDiffuse");
            info_inst->material_specular_loc_ = metadata.get_uniform_binding("uMaterialSpecular");
            info_inst->material_emission_loc_ = metadata.get_uniform_binding("uMaterialEmission");
            info_inst->material_shininess_loc_ = metadata.get_uniform_binding("uMaterialShininess");
            info_inst->global_ambient_loc_ = metadata.get_uniform_binding("uGlobalAmbient");

            if (fragment_statuses & egl_context_es1::FRAGMENT_STATE_ALPHA_TEST)
                info_inst->alpha_test_ref_loc_ = metadata.get_uniform_binding("uAlphaTestRef");

            if (fragment_statuses & egl_context_es1::FRAGMENT_STATE_FOG_ENABLE) {
                info_inst->fog_color_loc_ = metadata.get_uniform_binding("uFogColor");

                if ((fragment_statuses & egl_context_es1::FRAGMENT_STATE_FOG_MODE_MASK) == egl_context_es1::FRAGMENT_STATE_FOG_MODE_LINEAR) {
                    info_inst->fog_start_loc_ = metadata.get_uniform_binding("uFogStart");
                    info_inst->fog_end_loc_ = metadata.get_uniform_binding("uFogEnd");
                } else {
                    info_inst->fog_density_loc_ = metadata.get_uniform_binding("uFogDensity");
                }
            }

            if (fragment_statuses & egl_context_es1::FRAGMENT_STATE_LIGHTING_ENABLE) {
                std::string light_variable_pos_or_dir_name = "uLight0.mDirOrPosition";
                std::string light_ambient_name = "uLight0.mAmbient";
                std::string light_diffuse_name = "uLight0.mDiffuse";
                std::string light_specular_name = "uLight0.mSpecular";
                std::string light_spot_dir_name = "uLight0.mSpotDir";
                std::string light_spot_cutoff_name = "uLight0.mSpotCutoff";
                std::string light_spot_exponent_name = "uLight0.mSpotExponent";
                std::string light_attenuation_name = "uLight0.mAttenuation";

                for (std::uint32_t i = 0, mask = egl_context_es1::FRAGMENT_STATE_LIGHT0_ON; i < GLES1_EMU_MAX_LIGHT; i++, mask <<= 1) {
                    if (fragment_statuses & mask) {
                        char new_light_index_c = static_cast<char>('0' + i);

                        light_variable_pos_or_dir_name[6] = new_light_index_c;
                        light_ambient_name[6] = new_light_index_c;
                        light_diffuse_name[6] = new_light_index_c;
                        light_specular_name[6] = new_light_index_c;
                        light_spot_dir_name[6] = new_light_index_c;
                        light_spot_cutoff_name[6] = new_light_index_c;
                        light_spot_exponent_name[6] = new_light_index_c;
                        light_attenuation_name[6] = new_light_index_c;

                        info_inst->light_dir_or_pos_loc_[i] = metadata.get_uniform_binding(light_variable_pos_or_dir_name.c_str());
                        info_inst->light_ambient_loc_[i] = metadata.get_uniform_binding(light_ambient_name.c_str());
                        info_inst->light_diffuse_loc_[i] = metadata.get_uniform_binding(light_diffuse_name.c_str());
                        info_inst->light_specular_loc_[i] = metadata.get_uniform_binding(light_specular_name.c_str());
                        info_inst->light_spot_dir_loc_[i] = metadata.get_uniform_binding(light_spot_dir_name.c_str());
                        info_inst->light_spot_cutoff_loc_[i] = metadata.get_uniform_binding(light_spot_cutoff_name.c_str());
                        info_inst->light_spot_exponent_loc_[i] = metadata.get_uniform_binding(light_spot_exponent_name.c_str());
                        info_inst->light_attenuatation_vec_loc_[i] = metadata.get_uniform_binding(light_attenuation_name.c_str());
                    }
                }
            }
        }

        info = info_inst.get();
        program_cache_[vert_module][fragment_module] = { program_handle, std::move(info_inst) };

        return program_handle;
    }
}