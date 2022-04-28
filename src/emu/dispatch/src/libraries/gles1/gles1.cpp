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

#include <dispatch/dispatcher.h>
#include <drivers/graphics/graphics.h>
#include <services/window/screen.h>
#include <system/epoc.h>
#include <kernel/kernel.h>

#include <vector>

namespace eka2l1::dispatch {    
    std::string get_es1_extensions(drivers::graphics_driver *driver) {
        std::string original_list = GLES1_STATIC_STRING_EXTENSIONS;
        if (driver->support_extension(drivers::graphics_driver_extension_anisotrophy_filtering)) {
            original_list += "GL_EXT_texture_filter_anisotropic ";
        }
        return original_list;
    }

    static bool decompress_palette_data(std::vector<std::uint8_t> &dest, std::vector<std::size_t> &out_size, std::uint8_t *source, std::int32_t width,
        std::int32_t height, std::uint32_t source_format, std::int32_t mip_count, drivers::texture_format &dest_format,
        drivers::texture_data_type &dest_data_type, std::uint32_t &dest_format_gl) {
        std::uint8_t bytes_per_pixel = 0;

        switch (source_format) {
        case GL_PALETTE4_R5_G6_B5_OES_EMU:
        case GL_PALETTE8_R5_G6_B5_OES_EMU:
            dest_format = drivers::texture_format::rgb;
            dest_data_type = drivers::texture_data_type::ushort_5_6_5;
            dest_format_gl = GL_RGB_EMU;
            bytes_per_pixel = 2;
            break;

        case GL_PALETTE4_RGB5_A1_OES_EMU:
        case GL_PALETTE8_RGB5_A1_OES_EMU:
            dest_format = drivers::texture_format::rgba;
            dest_data_type = drivers::texture_data_type::ushort_5_5_5_1;
            bytes_per_pixel = 2;
            dest_format_gl = GL_RGBA_EMU;
            break;

        case GL_PALETTE4_RGB8_OES_EMU:
        case GL_PALETTE8_RGB8_OES_EMU:
            dest_format = drivers::texture_format::rgb;
            dest_data_type = drivers::texture_data_type::ubyte;
            bytes_per_pixel = 3;
            dest_format_gl = GL_RGB_EMU;
            break;
        
        case GL_PALETTE4_RGBA8_OES_EMU:
        case GL_PALETTE8_RGBA8_OES_EMU:
            dest_format = drivers::texture_format::rgba;
            dest_data_type = drivers::texture_data_type::ubyte;
            bytes_per_pixel = 4;
            dest_format_gl = GL_RGBA_EMU;
            break;

        case GL_PALETTE4_RGBA4_OES_EMU:
        case GL_PALETTE8_RGBA4_OES_EMU:
            dest_format = drivers::texture_format::rgba;
            dest_data_type = drivers::texture_data_type::ushort_4_4_4_4;
            bytes_per_pixel = 2;
            dest_format_gl = GL_RGBA_EMU;
            break;

        default:
            return false;
        }

        dest.clear();
        out_size.clear();

        std::uint8_t palette_bits = 8;

        if ((source_format >= GL_PALETTE4_RGB8_OES_EMU) && (source_format <= GL_PALETTE4_RGB5_A1_OES_EMU)) {
            palette_bits = 4;
        }

        while ((mip_count > 0) && (height > 0) && (width > 0)) {
            const std::size_t prev_size = dest.size();
            const std::size_t calculated_size = width * height * bytes_per_pixel;

            out_size.push_back(calculated_size);
            dest.resize(dest.size() + calculated_size);
            
            std::uint8_t *current_dest = dest.data() + prev_size;

            std::uint8_t *source_start = source + (1 << palette_bits) * bytes_per_pixel;
            for (std::int32_t y = 0; y < height; y++) {
                for (std::int32_t x = 0; x < width / (8 / palette_bits); x++) {
                    const std::int32_t current_pixel_idx = y * width + x;
                    const std::int32_t current_ptr_pixel_idx = current_pixel_idx * bytes_per_pixel;
                    const std::uint8_t indicies = source_start[current_pixel_idx];

                    if (palette_bits == 8) {
                        std::memcpy(current_dest + current_ptr_pixel_idx, source + indicies * bytes_per_pixel, bytes_per_pixel);
                    } else {
                        std::memcpy(current_dest + current_ptr_pixel_idx * 2, source + (indicies & 0xF) * bytes_per_pixel, bytes_per_pixel);
                        std::memcpy(current_dest + current_ptr_pixel_idx * 2 + bytes_per_pixel, source + ((indicies & 0xF0) >> 4) * bytes_per_pixel, bytes_per_pixel);
                    }
                }
            }

            mip_count--;
            width /= 2;
            height /= 2;
        }

        return true;
    }

    static bool convert_gl_factor_to_driver_enum(const std::uint32_t value, drivers::blend_factor &dest) {
        switch (value) {
        case GL_ONE_EMU:
            dest = drivers::blend_factor::one;
            break;

        case GL_ZERO_EMU:
            dest = drivers::blend_factor::zero;
            break;

        case GL_SRC_ALPHA_EMU:
            dest = drivers::blend_factor::frag_out_alpha;
            break;

        case GL_ONE_MINUS_SRC_ALPHA_EMU:
            dest = drivers::blend_factor::one_minus_frag_out_alpha;
            break;

        case GL_DST_ALPHA_EMU:
            dest = drivers::blend_factor::current_alpha;
            break;

        case GL_ONE_MINUS_DST_ALPHA_EMU:
            dest = drivers::blend_factor::one_minus_current_alpha;
            break;

        case GL_SRC_COLOR_EMU:
            dest = drivers::blend_factor::frag_out_color;
            break;

        case GL_ONE_MINUS_SRC_COLOR_EMU:
            dest = drivers::blend_factor::one_minus_frag_out_color;
            break;

        case GL_DST_COLOR:
            dest = drivers::blend_factor::current_color;
            break;

        case GL_ONE_MINUS_DST_COLOR:
            dest = drivers::blend_factor::one_minus_current_color;
            break;

        case GL_SRC_ALPHA_SATURATE_EMU:
            dest = drivers::blend_factor::frag_out_alpha_saturate;
            break;

        default:
            return false;
        }

        return true;
    }

    static bool cond_func_from_gl_enum(const std::uint32_t func, drivers::condition_func &drv_func) {
        switch (func) {
        case GL_ALWAYS_EMU:
            drv_func = drivers::condition_func::always;
            break;

        case GL_NEVER_EMU:
            drv_func = drivers::condition_func::never;
            break;

        case GL_GREATER_EMU:
            drv_func = drivers::condition_func::greater;
            break;

        case GL_GEQUAL_EMU:
            drv_func = drivers::condition_func::greater_or_equal;
            break;

        case GL_LESS_EMU:
            drv_func = drivers::condition_func::less;
            break;

        case GL_LEQUAL_EMU:
            drv_func = drivers::condition_func::less_or_equal;
            break;

        case GL_EQUAL_EMU:
            drv_func = drivers::condition_func::equal;
            break;

        case GL_NOTEQUAL_EMU:
            drv_func = drivers::condition_func::not_equal;
            break;

        default:
            return false;
        }

        return true;
    }

    static bool stencil_action_from_gl_enum(const std::uint32_t func, drivers::stencil_action &action) {
        switch (func) {
        case GL_KEEP_EMU:
            action = drivers::stencil_action::keep;
            break;
        
        case GL_ZERO_EMU:
            action = drivers::stencil_action::set_to_zero;
            break;

        case GL_REPLACE_EMU:
            action = drivers::stencil_action::replace;
            break;

        case GL_INCR_EMU:
            action = drivers::stencil_action::increment;
            break;

        case GL_DECR_EMU:
            action = drivers::stencil_action::decrement;
            break;

        case GL_INVERT_EMU:
            action = drivers::stencil_action::invert;
            break;

        default:
            return false;
        }

        return true;
    }

    gles1_buffer_pusher::gles1_buffer_pusher() {
        for (std::size_t i = 0; i < MAX_BUFFER_SLOT; i++) {
            buffers_[i] = 0;
            used_size_[i] = 0;
            data_[i] = nullptr;
        }

        current_buffer_ = 0;
        size_per_buffer_ = 0;
    }

    void gles1_buffer_pusher::initialize(drivers::graphics_driver *drv, const std::size_t size_per_buffer) {
        if (!size_per_buffer) {
            return;
        }
        size_per_buffer_ = size_per_buffer;
        for (std::uint8_t i = 0; i < MAX_BUFFER_SLOT; i++) {
            buffers_[i] = drivers::create_buffer(drv, nullptr, size_per_buffer, static_cast<drivers::buffer_upload_hint>(drivers::buffer_upload_dynamic | drivers::buffer_upload_draw));
            data_[i] = new std::uint8_t[size_per_buffer];
            used_size_[i] = 0;
        }
    }

    void gles1_buffer_pusher::destroy(drivers::graphics_command_builder &builder) {
        for (std::uint8_t i = 0; i < MAX_BUFFER_SLOT; i++) {
            builder.destroy(buffers_[i]);
            delete data_[i];
        }
    }

    void gles1_buffer_pusher::flush(drivers::graphics_command_builder &builder) {
        for (std::uint8_t i = 0; i <= current_buffer_; i++) {
            if (i == MAX_BUFFER_SLOT) {
                break;
            }

            const void *buffer_data_casted = data_[i];
            const std::uint32_t buffer_size = static_cast<std::uint32_t>(used_size_[i]);

            if (buffer_size == 0) {
                continue;
            }

            builder.update_buffer_data(buffers_[i], 0, 1, &buffer_data_casted, &buffer_size);
            used_size_[i] = 0;
        }

        // Move on, these others might still be used
        current_buffer_++;

        if (current_buffer_ >= MAX_BUFFER_SLOT) {
            // Force return...
            current_buffer_ = 0;
        }
    }

    void gles1_buffer_pusher::done_frame() {
        current_buffer_ = 0;
    }

    std::uint32_t get_gl_attrib_stride(const gles1_vertex_attrib &attrib) {
        std::uint32_t stride = attrib.stride_;
        if (!stride) {
            switch (attrib.data_type_) {
            case GL_BYTE_EMU:
            case GL_UNSIGNED_BYTE_EMU:
                stride = 1;
                break;

            case GL_SHORT_EMU:
            case GL_UNSIGNED_SHORT_EMU:
                stride = 2;
                break;

            case GL_FLOAT_EMU:
                stride = 4;
                break;

            case GL_FIXED_EMU:
                stride = 4;
                break;

            default:
                return 0;
            }

            stride *= attrib.size_;
        }

        return stride;
    }
    
    drivers::handle gles1_buffer_pusher::push_buffer(const std::uint8_t *data, const gles1_vertex_attrib &attrib, const std::int32_t first_index, const std::size_t vert_count, std::size_t &buffer_offset) {
        std::uint32_t stride = get_gl_attrib_stride(attrib);
        std::size_t total_buffer_size = stride * vert_count;

        return push_buffer(data + first_index * stride, total_buffer_size, buffer_offset);
    }

    drivers::handle gles1_buffer_pusher::push_buffer(const std::uint8_t *data_source, const std::size_t total_buffer_size, std::size_t &buffer_offset) {
        if (used_size_[current_buffer_] + total_buffer_size > size_per_buffer_) {
            current_buffer_++;
        }

        if (current_buffer_ == MAX_BUFFER_SLOT) {
            // No more slots, require flushing
            return 0;
        }

        buffer_offset = used_size_[current_buffer_];
        std::memcpy(data_[current_buffer_] + buffer_offset, data_source, total_buffer_size);

        used_size_[current_buffer_] += (((total_buffer_size + 3) / 4) * 4);
        return buffers_[current_buffer_];
    }

    gles_texture_unit::gles_texture_unit()
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
        : egl_context()
        , clear_depth_(1.0f)
        , clear_stencil_(0)
        , active_texture_unit_(0)
        , active_client_texture_unit_(0)
        , active_mat_stack_(GL_MODELVIEW_EMU)
        , binded_array_buffer_handle_(0)
        , binded_element_array_buffer_handle_(0)
        , current_palette_mat_index_(0)
        , active_cull_face_(drivers::rendering_face::back)
        , active_front_face_rule_(drivers::rendering_face_determine_rule::vertices_counter_clockwise)
        , source_blend_factor_(drivers::blend_factor::one)
        , dest_blend_factor_(drivers::blend_factor::zero)
        , attrib_changed_(false)
        , previous_first_index_(0)
        , skin_weights_per_ver(GLES1_EMU_MAX_WEIGHTS_PER_VERTEX)
        , material_shininess_(0.0f)
        , fog_density_(1.0f)
        , fog_start_(0.0f)
        , fog_end_(1.0f)
        , viewport_bl_(eka2l1::vec2(0, 0), eka2l1::vec2(0, 0))
        , scissor_bl_(eka2l1::vec2(0, 0), eka2l1::vec2(0, 0))
        , line_width_(1.0f)
        , vertex_statuses_(0)
        , fragment_statuses_(0)
        , non_shader_statuses_(0)
        , state_change_tracker_(0)
        , color_mask_(0b1111)
        , stencil_mask_(0xFFFFFFFF)
        , depth_mask_(0xFFFFFFFF)
        , depth_func_(GL_LESS_EMU)
        , stencil_func_(GL_ALWAYS_EMU)
        , stencil_func_mask_(0xFFFFFFFF)
        , stencil_func_ref_(0)
        , stencil_fail_action_(GL_KEEP_EMU)
        , stencil_depth_fail_action_(GL_KEEP_EMU)
        , stencil_depth_pass_action_(GL_KEEP_EMU)
        , alpha_test_ref_(0)
        , input_desc_(0)
        , polygon_offset_factor_(0.0f)
        , polygon_offset_units_(0.0f)
        , pack_alignment_(4)
        , unpack_alignment_(4)
        , depth_range_min_(0.0f)
        , depth_range_max_(1.0f) {
        clear_color_[0] = 0.0f;
        clear_color_[1] = 0.0f;
        clear_color_[2] = 0.0f;
        clear_color_[3] = 0.0f;

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

    void egl_context_es1::destroy(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder) {
        if (input_desc_) {
            cmd_builder_.destroy(input_desc_);
        }

        vertex_buffer_pusher_.destroy(builder);
        index_buffer_pusher_.destroy(builder);

        for (auto &obj: objects_) {
            if (obj) {
                obj.reset();
            }
        }

        while (!texture_pools_.empty()) {
            drivers::handle h = texture_pools_.top();
            builder.destroy(h);

            texture_pools_.pop();
        }
        
        while (!buffer_pools_.empty()) {
            drivers::handle h = buffer_pools_.top();
            builder.destroy(h);

            buffer_pools_.pop();
        }

        egl_context::destroy(driver, builder);
    }

    void egl_context_es1::flush_to_driver(drivers::graphics_driver *drv, const bool is_frame_swap_flush) {
        drivers::graphics_command_builder transfer_builder;
        vertex_buffer_pusher_.flush(transfer_builder);
        index_buffer_pusher_.flush(transfer_builder);

        drivers::command_list retrieved = transfer_builder.retrieve_command_list();
        drv->submit_command_list(retrieved);

        flush_state_changes();
        retrieved = cmd_builder_.retrieve_command_list();

        drv->submit_command_list(retrieved);
        init_context_state();

        if (is_frame_swap_flush) {
            vertex_buffer_pusher_.done_frame();
            index_buffer_pusher_.done_frame();
        }
    }

    void egl_context_es1::on_surface_changed(egl_surface *prev_read, egl_surface *prev_draw) {
        // Rebind viewport
        viewport_bl_.top = eka2l1::vec2(0, 0);
        viewport_bl_.size = draw_surface_->dimension_;
    }

    void egl_context_es1::flush_state_changes() {
        // Beside texture parameters still being submitted directly. This is the best for now. 
        if (state_change_tracker_ == 0) {
            return;
        }

        if (state_change_tracker_ & STATE_CHANGED_CULL_FACE) {
            cmd_builder_.set_cull_face(active_cull_face_);
        }

        if (state_change_tracker_ & STATE_CHANGED_SCISSOR_RECT) {
            eka2l1::rect scissor_bl_scaled = scissor_bl_;
            scissor_bl_scaled.scale(draw_surface_->current_scale_);
            scissor_bl_scaled.size.y *= -1;

            cmd_builder_.clip_rect(scissor_bl_scaled);
        }

        if (state_change_tracker_ & STATE_CHANGED_FRONT_FACE_RULE) {
            cmd_builder_.set_front_face_rule(active_front_face_rule_);
        }

        if (state_change_tracker_ & STATE_CHANGED_VIEWPORT_RECT) {
            eka2l1::rect viewport_transformed = viewport_bl_;

            // Make viewport bottom left
            viewport_transformed.scale(draw_surface_->current_scale_);
            viewport_transformed.size.y *= -1;

            cmd_builder_.set_viewport(viewport_transformed);
        }

        if (state_change_tracker_ & STATE_CHANGED_COLOR_MASK) {
            cmd_builder_.set_color_mask(color_mask_);
        }

        if (state_change_tracker_ & STATE_CHANGED_DEPTH_BIAS) {    
            cmd_builder_.set_depth_bias(polygon_offset_units_, 1.0, polygon_offset_factor_);
        }

        if (state_change_tracker_ & STATE_CHANGED_STENCIL_MASK) {
            cmd_builder_.set_stencil_mask(drivers::rendering_face::back_and_front, stencil_mask_);
        }

        if (state_change_tracker_ & STATE_CHANGED_BLEND_FACTOR) {
            cmd_builder_.blend_formula(drivers::blend_equation::add, drivers::blend_equation::add, source_blend_factor_, dest_blend_factor_,
                source_blend_factor_, dest_blend_factor_);
        }

        if (state_change_tracker_ & STATE_CHANGED_LINE_WIDTH) {    
            cmd_builder_.set_line_width(line_width_);
        }

        if (state_change_tracker_ & STATE_CHANGED_DEPTH_MASK) {    
            cmd_builder_.set_depth_mask(depth_mask_);
        }

        if (state_change_tracker_ & STATE_CHANGED_DEPTH_PASS_COND) {
            drivers::condition_func func;
            cond_func_from_gl_enum(depth_func_, func);

            cmd_builder_.set_depth_pass_condition(func);
        }

        if (state_change_tracker_ & STATE_CHANGED_DEPTH_RANGE) {
            cmd_builder_.set_depth_range(depth_range_min_, depth_range_max_);
        }

        if (state_change_tracker_ & STATE_CHANGED_STENCIL_FUNC) {
            drivers::condition_func stencil_func_drv;
            cond_func_from_gl_enum(stencil_func_, stencil_func_drv);

            cmd_builder_.set_stencil_pass_condition(drivers::rendering_face::back_and_front, stencil_func_drv,
                stencil_func_ref_, stencil_func_mask_);
        }

        if (state_change_tracker_ & STATE_CHANGED_STENCIL_OP) {
            drivers::stencil_action stencil_action_fail_drv, stencil_action_depth_fail_drv, stencil_action_depth_pass_drv;
            stencil_action_from_gl_enum(stencil_fail_action_, stencil_action_fail_drv);
            stencil_action_from_gl_enum(stencil_depth_fail_action_, stencil_action_depth_fail_drv);
            stencil_action_from_gl_enum(stencil_depth_pass_action_, stencil_action_depth_pass_drv);
    
            cmd_builder_.set_stencil_action(drivers::rendering_face::back_and_front, stencil_action_fail_drv,
                stencil_action_depth_fail_drv, stencil_action_depth_pass_drv);
        }

        state_change_tracker_ = 0;
    }

    void egl_context_es1::init_context_state() {
        cmd_builder_.bind_bitmap(draw_surface_->handle_, read_surface_->handle_);
        cmd_builder_.set_cull_face(active_cull_face_);
        cmd_builder_.set_front_face_rule(active_front_face_rule_);
        cmd_builder_.set_color_mask(color_mask_);
        cmd_builder_.set_stencil_mask(drivers::rendering_face::back_and_front, stencil_mask_);

        drivers::condition_func depth_func_drv;
        cond_func_from_gl_enum(depth_func_, depth_func_drv);

        drivers::condition_func stencil_func_drv;
        cond_func_from_gl_enum(stencil_func_, stencil_func_drv);

        drivers::stencil_action stencil_action_fail_drv, stencil_action_depth_fail_drv, stencil_action_depth_pass_drv;
        stencil_action_from_gl_enum(stencil_fail_action_, stencil_action_fail_drv);
        stencil_action_from_gl_enum(stencil_depth_fail_action_, stencil_action_depth_fail_drv);
        stencil_action_from_gl_enum(stencil_depth_pass_action_, stencil_action_depth_pass_drv);

        cmd_builder_.set_depth_pass_condition(depth_func_drv);
        cmd_builder_.set_depth_mask(depth_mask_);
        cmd_builder_.blend_formula(drivers::blend_equation::add, drivers::blend_equation::add, source_blend_factor_, dest_blend_factor_,
            source_blend_factor_, dest_blend_factor_);

        cmd_builder_.set_line_width(line_width_);
        cmd_builder_.set_depth_bias(polygon_offset_units_, 1.0, polygon_offset_factor_);
        cmd_builder_.set_depth_range(depth_range_min_, depth_range_max_);
        cmd_builder_.set_stencil_pass_condition(drivers::rendering_face::back_and_front, stencil_func_drv,
            stencil_func_ref_, stencil_func_mask_);
        cmd_builder_.set_stencil_action(drivers::rendering_face::back_and_front, stencil_action_fail_drv,
            stencil_action_depth_fail_drv, stencil_action_depth_pass_drv);

        cmd_builder_.set_feature(drivers::graphics_feature::blend, non_shader_statuses_ & NON_SHADER_STATE_BLEND_ENABLE);
        cmd_builder_.set_feature(drivers::graphics_feature::clipping, non_shader_statuses_ & NON_SHADER_STATE_SCISSOR_ENABLE);
        cmd_builder_.set_feature(drivers::graphics_feature::cull, non_shader_statuses_ & NON_SHADER_STATE_CULL_FACE_ENABLE);
        cmd_builder_.set_feature(drivers::graphics_feature::depth_test, non_shader_statuses_ & NON_SHADER_STATE_DEPTH_TEST_ENABLE);
        cmd_builder_.set_feature(drivers::graphics_feature::dither, non_shader_statuses_ & NON_SHADER_STATE_DITHER);
        cmd_builder_.set_feature(drivers::graphics_feature::line_smooth, non_shader_statuses_ & NON_SHADER_STATE_LINE_SMOOTH);
        cmd_builder_.set_feature(drivers::graphics_feature::multisample, non_shader_statuses_ & NON_SHADER_STATE_MULTISAMPLE);
        cmd_builder_.set_feature(drivers::graphics_feature::polygon_offset_fill, non_shader_statuses_ & NON_SHADER_STATE_POLYGON_OFFSET_FILL);
        cmd_builder_.set_feature(drivers::graphics_feature::sample_alpha_to_coverage, non_shader_statuses_ & NON_SHADER_STATE_SAMPLE_ALPHA_TO_COVERAGE);
        cmd_builder_.set_feature(drivers::graphics_feature::sample_alpha_to_one, non_shader_statuses_ & NON_SHADER_STATE_SAMPLE_ALPHA_TO_ONE);
        cmd_builder_.set_feature(drivers::graphics_feature::sample_coverage, non_shader_statuses_ & NON_SHADER_STATE_SAMPLE_COVERAGE);
        cmd_builder_.set_feature(drivers::graphics_feature::stencil_test, non_shader_statuses_ & NON_SHADER_STATE_STENCIL_TEST_ENABLE);

        eka2l1::rect viewport_transformed = viewport_bl_;
        eka2l1::rect clip_rect_transformed = scissor_bl_;

        viewport_transformed.scale(draw_surface_->current_scale_);
        viewport_transformed.size.y *= -1;

        // Scale and make scissor bottom left.
        clip_rect_transformed.scale(draw_surface_->current_scale_);
        clip_rect_transformed.size.y *= -1;

        cmd_builder_.clip_rect(clip_rect_transformed);
        cmd_builder_.set_viewport(viewport_transformed);
        
        // Some games have 0 alphas in some situation!
        // TODO: This is hack definitely. So removal is ideal.
        switch (draw_surface_->backed_screen_->disp_mode) {
        case epoc::display_mode::color16m:
        case epoc::display_mode::color16mu:
        case epoc::display_mode::color16ma:
            cmd_builder_.set_swizzle(draw_surface_->handle_, drivers::channel_swizzle::red, drivers::channel_swizzle::green,
                drivers::channel_swizzle::blue, drivers::channel_swizzle::one);

            break;

        default:
            break;
        }
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

    gles1_driver_texture *egl_context_es1::binded_texture() {
        if (texture_units_[active_texture_unit_].binded_texture_handle_ == 0) {
            return nullptr;
        }

        std::uint32_t handle = texture_units_[active_texture_unit_].binded_texture_handle_;

        auto *obj = objects_.get(handle);
        if (!obj || !obj->get() || (*obj)->object_type() != GLES1_OBJECT_TEXTURE) {
            if (!obj->get()) {
                // The capacity is still enough. Someone has deleted the texture that should not be ! (yes, Pet Me by mBounce)
                LOG_WARN(HLE_DISPATCHER, "Texture name {} was previously deleted, generate a new one"
                    " (only because the slot is empty)!", handle);
                *obj = std::make_unique<gles1_driver_texture>(this);
            } else {
                return nullptr;
            }
        }

        return reinterpret_cast<gles1_driver_texture*>(obj->get());
    }

    gles1_driver_buffer *egl_context_es1::binded_buffer(const bool is_array_buffer) {
        auto *obj = objects_.get(is_array_buffer ? binded_array_buffer_handle_ : binded_element_array_buffer_handle_);
        if (!obj || (*obj)->object_type() != GLES1_OBJECT_BUFFER) {
            return nullptr;
        }

        return reinterpret_cast<gles1_driver_buffer*>(obj->get());
    }

    void egl_context_es1::return_handle_to_pool(const gles1_object_type type, const drivers::handle h) {
        switch (type) {
        case GLES1_OBJECT_BUFFER:
            buffer_pools_.push(h);
            break;

        case GLES1_OBJECT_TEXTURE:
            texture_pools_.push(h);
            break;

        default:
            break;
        }
    }

    gles1_driver_object::gles1_driver_object(egl_context_es1 *ctx)
        : context_(ctx)
        , driver_handle_(0) {
    }

    gles1_driver_texture::~gles1_driver_texture() {
        if (driver_handle_ != 0) {
            if (context_) {
                context_->return_handle_to_pool(GLES1_OBJECT_TEXTURE, driver_handle_);
            }
        }
    }
    
    gles1_driver_buffer::~gles1_driver_buffer() {
        if (driver_handle_ != 0) {
            if (context_) {
                context_->return_handle_to_pool(GLES1_OBJECT_BUFFER, driver_handle_);
            }
        }
    }

    gles1_driver_texture::gles1_driver_texture(egl_context_es1 *ctx)
        : gles1_driver_object(ctx)
        , internal_format_(0)
        , min_filter_(GL_NEAREST_MIPMAP_LINEAR_EMU)
        , mag_filter_(GL_LINEAR_EMU)
        , wrap_s_(GL_REPEAT_EMU)
        , wrap_t_(GL_REPEAT_EMU)
        , mip_count_(0)
        , auto_regen_mipmap_(false)
        , max_anisotrophy_(-1.0f) {
    }

    gles1_driver_buffer::gles1_driver_buffer(egl_context_es1 *ctx)
        : gles1_driver_object(ctx)
        , data_size_(0) {
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

    BRIDGE_FUNC_LIBRARY(void, gl_clear_color_emu, float red, float green, float blue, float alpha) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->clear_color_[0] = red;
        ctx->clear_color_[1] = green;
        ctx->clear_color_[2] = blue;
        ctx->clear_color_[3] = alpha;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_clear_colorx_emu, gl_fixed red, gl_fixed green, gl_fixed blue, gl_fixed alpha) {
        gl_clear_color_emu(sys, FIXED_32_TO_FLOAT(red), FIXED_32_TO_FLOAT(green), FIXED_32_TO_FLOAT(blue), FIXED_32_TO_FLOAT(alpha));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_clear_depthf_emu, float depth) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->clear_depth_ = depth;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_clear_depthx_emu, gl_fixed depth) {
        gl_clear_depthf_emu(sys, FIXED_32_TO_FLOAT(depth));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_clear_stencil, std::int32_t s) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->clear_stencil_ = s;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_clear_emu, std::uint32_t bits) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        if (bits == 0) {
            return;
        }

        eka2l1::vecx<float, 6> clear_parameters;

        std::uint32_t flags_driver = 0;
        if (bits & GL_COLOR_BUFFER_BIT_EMU) {
            flags_driver |= drivers::draw_buffer_bit_color_buffer;

            clear_parameters[0] = ctx->clear_color_[0];
            clear_parameters[1] = ctx->clear_color_[1];
            clear_parameters[2] = ctx->clear_color_[2];
            clear_parameters[3] = ctx->clear_color_[3];
        }

        if (bits & GL_DEPTH_BUFFER_BIT_EMU) {
            flags_driver |= drivers::draw_buffer_bit_depth_buffer;
            clear_parameters[4] = ctx->clear_depth_;
        }

        if (bits & GL_STENCIL_BUFFER_BIT_EMU) {
            flags_driver |= drivers::draw_buffer_bit_stencil_buffer;
            clear_parameters[5] = ctx->clear_stencil_ / 255.0f;
        }

        ctx->flush_state_changes();
        ctx->cmd_builder_.clear(clear_parameters, flags_driver);

        if (ctx->cmd_builder_.need_flush()) {
            ctx->flush_to_driver(sys->get_graphics_driver());
        }
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
    
    BRIDGE_FUNC_LIBRARY(std::uint32_t, gl_get_error_emu) {
        dispatcher *dp = sys->get_dispatcher();
        kernel_system *kern = sys->get_kernel_system();

        if (!dp || !kern) {
            return GL_INVALID_OPERATION;
        }

        kernel::thread *crr_thread = kern->crr_thread();
        if (!crr_thread) {
            return GL_INVALID_OPERATION;
        }

        dispatch::egl_controller &controller = dp->get_egl_controller();
        return controller.pop_error(crr_thread->unique_id());
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

    BRIDGE_FUNC_LIBRARY(void, gl_cull_face_emu, std::uint32_t mode) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (mode) {
        case GL_BACK_EMU:
            ctx->active_cull_face_ = drivers::rendering_face::back;
            break;

        case GL_FRONT_EMU:
            ctx->active_cull_face_ = drivers::rendering_face::front;
            break;

        case GL_FRONT_AND_BACK_EMU:
            ctx->active_cull_face_ = drivers::rendering_face::back_and_front;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        ctx->state_change_tracker_ |= egl_context_es1::STATE_CHANGED_CULL_FACE;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_scissor_emu, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        // Emulator graphics abstraction use top-left as origin.
        ctx->scissor_bl_.top = eka2l1::vec2(x, y);
        ctx->scissor_bl_.size = eka2l1::vec2(width, height);

        ctx->state_change_tracker_ |= egl_context_es1::STATE_CHANGED_SCISSOR_RECT;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_front_face_emu, std::uint32_t mode) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (mode) {
        case GL_CLOCKWISE_EMU:
            ctx->active_front_face_rule_ = drivers::rendering_face_determine_rule::vertices_clockwise;
            break;

        case GL_COUNTER_CLOCKWISE_EMU:
            ctx->active_front_face_rule_ = drivers::rendering_face_determine_rule::vertices_counter_clockwise;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        ctx->state_change_tracker_ |= egl_context_es1::STATE_CHANGED_FRONT_FACE_RULE;
    }
    
    BRIDGE_FUNC_LIBRARY(bool, gl_is_texture_emu, std::uint32_t name) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return false;
        }

        gles1_driver_object_instance *obj = ctx->objects_.get(name);
        return obj && ((*obj)->object_type() == GLES1_OBJECT_TEXTURE);
    }

    BRIDGE_FUNC_LIBRARY(bool, gl_is_buffer_emu, std::uint32_t name) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return false;
        }

        gles1_driver_object_instance *obj = ctx->objects_.get(name);
        return obj && ((*obj)->object_type() == GLES1_OBJECT_BUFFER);
    }
    
    static void gen_gles1_objects_generic(system *sys, gles1_object_type obj_type, std::int32_t n, std::uint32_t *texs) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (n < 0 || !texs) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (n == 0) {
            return;
        }

        gles1_driver_object_instance stub_obj;

        for (std::int32_t i = 0; i < n; i++) {
            switch (obj_type) {
            case GLES1_OBJECT_TEXTURE:
                stub_obj = std::make_unique<gles1_driver_texture>(ctx);
                break;

            case GLES1_OBJECT_BUFFER:
                stub_obj = std::make_unique<gles1_driver_buffer>(ctx);
                break;

            default:
                break;
            }
            texs[i] = static_cast<std::uint32_t>(ctx->objects_.add(stub_obj));
        }
    }

    static void delete_gles1_objects_generic(system *sys, gles1_object_type obj_type, std::int32_t n, std::uint32_t *names) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        drivers::graphics_driver *drv = sys->get_graphics_driver();

        if (n < 0 || !names) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (n == 0) {
            return;
        }

        for (std::int32_t i = 0; i < n; i++) {
            auto *obj = ctx->objects_.get(names[i]);
            if (obj && *obj && ((*obj)->object_type() == obj_type)) {
                ctx->objects_.remove(names[i]);
            }
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_gen_textures_emu, std::int32_t n, std::uint32_t *texs) {
        gen_gles1_objects_generic(sys, GLES1_OBJECT_TEXTURE, n, texs);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_gen_buffers_emu, std::int32_t n, std::uint32_t *buffers) {
        gen_gles1_objects_generic(sys, GLES1_OBJECT_BUFFER, n, buffers);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_delete_textures_emu, std::int32_t n, std::uint32_t *texs) {
        delete_gles1_objects_generic(sys, GLES1_OBJECT_TEXTURE, n, texs);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_delete_buffers_emu, std::int32_t n, std::uint32_t *buffers) {
        delete_gles1_objects_generic(sys, GLES1_OBJECT_BUFFER, n, buffers);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_bind_texture_emu, std::uint32_t target, std::uint32_t name) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_TEXTURE_2D_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->texture_units_[ctx->active_texture_unit_].binded_texture_handle_ = name;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_bind_buffer_emu, std::uint32_t target, std::uint32_t name) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (target) {
        case GL_ARRAY_BUFFER_EMU:
            ctx->binded_array_buffer_handle_ = name;
            break;

        case GL_ELEMENT_ARRAY_BUFFER_EMU:
            ctx->binded_element_array_buffer_handle_ = name;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    static inline std::uint32_t is_format_and_data_type_ok(const std::uint32_t format, const std::uint32_t data_type) {
        if ((format != GL_ALPHA_EMU) && (format != GL_RGB_EMU) && (format != GL_RGBA_EMU) && (format != GL_LUMINANCE_EMU)
            && (format != GL_LUMINANCE_ALPHA_EMU)) {
            return GL_INVALID_ENUM;
        }

        if ((data_type != GL_UNSIGNED_BYTE_EMU) && (data_type != GL_UNSIGNED_SHORT_4_4_4_4_EMU) && (data_type != GL_UNSIGNED_SHORT_5_5_5_1_EMU)
            && (data_type != GL_UNSIGNED_SHORT_5_6_5_EMU)) {
            return GL_INVALID_ENUM;
        }

        if ((data_type == GL_UNSIGNED_SHORT_5_6_5_EMU) && (format != GL_RGB_EMU)) {
            return GL_INVALID_OPERATION;
        }

        if (((data_type == GL_UNSIGNED_SHORT_5_5_5_1_EMU) || (data_type == GL_UNSIGNED_SHORT_4_4_4_4_EMU)) && (format != GL_RGBA_EMU)) {
            return GL_INVALID_OPERATION;
        }

        return 0;
    }

    static bool is_valid_gl_emu_func(const std::uint32_t func) {
        return ((func == GL_NEVER_EMU) || (func == GL_ALWAYS_EMU) || (func == GL_GREATER_EMU) || (func == GL_GEQUAL_EMU) ||
            (func == GL_LESS_EMU) || (func == GL_LEQUAL_EMU) || (func == GL_EQUAL_EMU) || (func == GL_NOTEQUAL_EMU));
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

    // This will only works on GLES1, they don't have swizzle masks yet.
    static void get_data_type_to_upload(drivers::texture_format &internal_out, drivers::texture_format &format_out, drivers::texture_data_type &type_out,
        drivers::channel_swizzles &swizzles_out, const std::uint32_t format, const std::uint32_t data_type) {
        switch (format) {
        case GL_RGB_EMU:
            format_out = drivers::texture_format::rgb;
            internal_out = format_out;
            swizzles_out = { drivers::channel_swizzle::red, drivers::channel_swizzle::green, drivers::channel_swizzle::blue, drivers::channel_swizzle::one };
            break;

        case GL_RGBA_EMU:
            format_out = drivers::texture_format::rgba;
            internal_out = format_out;
            swizzles_out = { drivers::channel_swizzle::red, drivers::channel_swizzle::green, drivers::channel_swizzle::blue, drivers::channel_swizzle::alpha };
            break;

        case GL_ALPHA_EMU:
            format_out = drivers::texture_format::r;
            internal_out = drivers::texture_format::r8;
            swizzles_out = { drivers::channel_swizzle::zero, drivers::channel_swizzle::zero, drivers::channel_swizzle::zero, drivers::channel_swizzle::red };
            break;

        case GL_LUMINANCE_EMU:
            format_out = drivers::texture_format::r;
            internal_out = drivers::texture_format::r8;
            swizzles_out = { drivers::channel_swizzle::red, drivers::channel_swizzle::red, drivers::channel_swizzle::red, drivers::channel_swizzle::one };
            break;
            
        case GL_LUMINANCE_ALPHA_EMU:
            format_out = drivers::texture_format::rg;
            internal_out = drivers::texture_format::rg8;
            swizzles_out = { drivers::channel_swizzle::red, drivers::channel_swizzle::red, drivers::channel_swizzle::red, drivers::channel_swizzle::green };
            break;

        default:
            break;
        }

        switch (data_type) {
        case GL_UNSIGNED_BYTE_EMU:
            type_out = drivers::texture_data_type::ubyte;
            break;

        case GL_UNSIGNED_SHORT_4_4_4_4_EMU:
            type_out = drivers::texture_data_type::ushort_4_4_4_4;
            break;

        case GL_UNSIGNED_SHORT_5_5_5_1_EMU:
            type_out = drivers::texture_data_type::ushort_5_5_5_1;
            break;

        case GL_UNSIGNED_SHORT_5_6_5_EMU:
            type_out = drivers::texture_data_type::ushort_5_6_5;
            break;

        default:
            break;
        }
    }

    static std::uint32_t calculate_possible_upload_size(const eka2l1::vec2 size, const std::uint32_t format, const std::uint32_t data_type) {
        if ((format == GL_LUMINANCE_ALPHA_EMU) || (format == GL_LUMINANCE_EMU) || (format == GL_ALPHA_EMU)) {
            return size.x * size.y;
        }

        if (format == GL_RGB_EMU) {
            if (data_type == GL_UNSIGNED_BYTE_EMU) {
                return size.x * size.y * 3;
            } else {
                return size.x * size.y * 2;
            }
        } else {
            if (format == GL_RGBA_EMU) {
                if (data_type == GL_UNSIGNED_BYTE_EMU) {
                    return size.x * size.y * 4;
                } else {
                    return size.x * size.y * 2;
                }
            }
        }

        return 0;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_compressed_tex_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t internal_format,
        std::int32_t width, std::int32_t height, std::int32_t border, std::uint32_t image_size, void *data_pixels) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_TEXTURE_2D_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if ((width < 0) || (height < 0) || (width > GLES1_EMU_MAX_TEXTURE_SIZE) || (height > GLES1_EMU_MAX_TEXTURE_SIZE) || !data_pixels) {
            controller.push_error(ctx, GL_INVALID_VALUE);
        }

        gles1_driver_texture *tex = ctx->binded_texture();
        if (!tex) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        bool need_reinstantiate = true;

        drivers::texture_format internal_format_driver;

        if ((internal_format >= GL_PALETTE4_RGBA8_OES_EMU) && (internal_format <= GL_PALETTE8_RGB5_A1_OES_EMU)) {
            if (level > 0) {
                controller.push_error(ctx, GL_INVALID_ENUM);
            } else {
                level = (-level) + 1;

                if (level > GLES1_EMU_MAX_TEXTURE_MIP_LEVEL) {
                    controller.push_error(ctx, GL_INVALID_VALUE);
                }
            }

            std::vector<std::uint8_t> decompressed_data;
            std::vector<std::size_t> out_size;

            std::uint32_t format_gl = 0;
            drivers::texture_data_type dtype;

            if (!decompress_palette_data(decompressed_data, out_size, reinterpret_cast<std::uint8_t*>(data_pixels), width, height, internal_format,
                level, internal_format_driver, dtype, format_gl)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            std::uint8_t *data_to_pass = decompressed_data.data();

            // TODO: border is ignored!
            if (!tex->handle_value()) {
                if (!ctx->texture_pools_.empty()) {
                    tex->assign_handle(ctx->texture_pools_.top());
                    ctx->texture_pools_.pop();
                } else {
                    drivers::graphics_driver *drv = sys->get_graphics_driver();
                    drivers::handle new_h = drivers::create_texture(drv, 2, 0, internal_format_driver,
                        internal_format_driver, dtype, data_to_pass, out_size[0], eka2l1::vec3(width, height, 0),
                        0, ctx->unpack_alignment_);

                    if (!new_h) {
                        controller.push_error(ctx, GL_INVALID_OPERATION);
                        return;
                    }

                    need_reinstantiate = false;
                    tex->assign_handle(new_h);
                }
            }

            if (need_reinstantiate) {
                ctx->cmd_builder_.recreate_texture(tex->handle_value(), 2, 0, internal_format_driver,
                    internal_format_driver, dtype, data_to_pass, out_size[0], eka2l1::vec3(width, height, 0),
                    0, ctx->unpack_alignment_);
            }

            data_to_pass += out_size[0];

            for (std::size_t i = 1; i < out_size.size(); i++) {
                width /= 2;
                height /= 2;

                ctx->cmd_builder_.recreate_texture(tex->handle_value(), 2, static_cast<std::uint8_t>(i), internal_format_driver,
                    internal_format_driver, dtype, data_to_pass, out_size[i], eka2l1::vec3(width, height, 0), 0, ctx->unpack_alignment_);            
            }

            tex->set_mip_count(static_cast<std::uint32_t>(level));
            ctx->cmd_builder_.set_texture_max_mip(tex->handle_value(), static_cast<std::uint32_t>(level));
        } else {
            // Pass to driver as normal
            switch (internal_format) {
            case GL_ETC1_RGB8_OES_EMU:
                // Backwards compatible!!!
                internal_format_driver = drivers::texture_format::etc2_rgb8;
                break;

            case GL_COMPRESSED_RGB_PVRTC_4BPPV1_EMU:
                internal_format_driver = drivers::texture_format::pvrtc_4bppv1_rgb;
                break;

            case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_EMU:
                internal_format_driver = drivers::texture_format::pvrtc_4bppv1_rgba;
                break;

            case GL_COMPRESSED_RGB_PVRTC_2BPPV1_EMU:
                internal_format_driver = drivers::texture_format::pvrtc_2bppv1_rgb;
                break;

            case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_EMU:
                internal_format_driver = drivers::texture_format::pvrtc_2bppv1_rgba;
                break;

            default:
                LOG_ERROR(HLE_DISPATCHER, "Unrecognised internal format 0x{:X} for compressed texture!", internal_format);
                controller.push_error(ctx, GL_INVALID_ENUM);

                return;
            }

            if (level < 0) {
                controller.push_error(ctx, GL_INVALID_VALUE);
                return;
            }
            
            if (!tex->handle_value()) {
                if (!ctx->texture_pools_.empty()) {
                    tex->assign_handle(ctx->texture_pools_.top());
                    ctx->texture_pools_.pop();
                } else {
                    drivers::graphics_driver *drv = sys->get_graphics_driver();
                    drivers::handle new_h = drivers::create_texture(drv, 2, static_cast<std::uint8_t>(level), internal_format_driver,
                        internal_format_driver, drivers::texture_data_type::compressed, data_pixels, image_size, eka2l1::vec3(width, height, 0),
                        0, ctx->unpack_alignment_);

                    if (!new_h) {
                        controller.push_error(ctx, GL_INVALID_OPERATION);
                        return;
                    }

                    need_reinstantiate = false;
                    tex->assign_handle(new_h);
                }
            }

            if (need_reinstantiate) {
                ctx->cmd_builder_.recreate_texture(tex->handle_value(), 2, static_cast<std::uint8_t>(level), internal_format_driver,
                    internal_format_driver, drivers::texture_data_type::compressed, data_pixels, image_size, eka2l1::vec3(width, height, 0),
                    0, ctx->unpack_alignment_);
            }

            tex->set_mip_count(common::max(tex->get_mip_count(), static_cast<std::uint32_t>(level)));
            ctx->cmd_builder_.set_texture_max_mip(tex->handle_value(), tex->get_mip_count());
        }

        tex->set_internal_format(internal_format);
        tex->set_size(eka2l1::vec2(width, height));

        if (tex->auto_regenerate_mipmap()) {
            ctx->cmd_builder_.regenerate_mips(tex->handle_value());
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t internal_format,
        std::int32_t width, std::int32_t height, std::int32_t border, std::uint32_t format, std::uint32_t data_type,
        void *data_pixels) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_TEXTURE_2D_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if ((level < 0) || (level > GLES1_EMU_MAX_TEXTURE_MIP_LEVEL) || (width < 0) || (height < 0) || (width > GLES1_EMU_MAX_TEXTURE_SIZE)
            || (height > GLES1_EMU_MAX_TEXTURE_SIZE) || !data_pixels) {
            controller.push_error(ctx, GL_INVALID_VALUE);
        }

        if (internal_format != format) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        const std::uint32_t error = is_format_and_data_type_ok(format, data_type);
        if (error != 0) {
            controller.push_error(ctx, error);
            return;
        }

        gles1_driver_texture *tex = ctx->binded_texture();
        if (!tex) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        bool need_reinstantiate = true;

        drivers::texture_format internal_format_driver;
        drivers::texture_format format_driver;
        drivers::texture_data_type dtype;
        drivers::channel_swizzles swizzles;

        get_data_type_to_upload(internal_format_driver, format_driver, dtype, swizzles, format, data_type);

        // TODO: border is ignored!
        if (!tex->handle_value()) {
            if (!ctx->texture_pools_.empty()) {
                tex->assign_handle(ctx->texture_pools_.top());
                ctx->texture_pools_.pop();
            } else {
                drivers::graphics_driver *drv = sys->get_graphics_driver();
                drivers::handle new_h = drivers::create_texture(drv, 2, static_cast<std::uint8_t>(level), internal_format_driver,
                    format_driver, dtype, data_pixels, 0, eka2l1::vec3(width, height, 0), 0, ctx->unpack_alignment_);

                if (!new_h) {
                    controller.push_error(ctx, GL_INVALID_OPERATION);
                    return;
                }

                need_reinstantiate = false;
                tex->assign_handle(new_h);
            }
        }

        if (need_reinstantiate) {
            const std::size_t needed_size = calculate_possible_upload_size(eka2l1::vec2(width, height), format, data_type);
            ctx->cmd_builder_.recreate_texture(tex->handle_value(), 2, static_cast<std::uint8_t>(level), internal_format_driver,
                format_driver, dtype, data_pixels, needed_size, eka2l1::vec3(width, height, 0), 0, ctx->unpack_alignment_);
        }

        tex->set_internal_format(format);
        tex->set_size(eka2l1::vec2(width, height));
        tex->set_mip_count(common::max(tex->get_mip_count(), static_cast<std::uint32_t>(level)));

        if (tex->auto_regenerate_mipmap()) {
            ctx->cmd_builder_.regenerate_mips(tex->handle_value());
        }

        ctx->cmd_builder_.set_texture_max_mip(tex->handle_value(), tex->get_mip_count());
        ctx->cmd_builder_.set_swizzle(tex->handle_value(), swizzles[0], swizzles[1], swizzles[2], swizzles[3]);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_tex_sub_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t xoffset,
        std::int32_t yoffset, std::int32_t width, std::int32_t height, std::uint32_t format, std::uint32_t data_type,
        void *data_pixels) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_TEXTURE_2D_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if ((level < 0) || (level > GLES1_EMU_MAX_TEXTURE_MIP_LEVEL) || (width < 0) || (height < 0) || (width > GLES1_EMU_MAX_TEXTURE_SIZE)
            || (height > GLES1_EMU_MAX_TEXTURE_SIZE) || !data_pixels) {
            controller.push_error(ctx, GL_INVALID_VALUE);
        }

        const std::uint32_t error = is_format_and_data_type_ok(format, data_type);
        if (error != 0) {
            controller.push_error(ctx, error);
            return;
        }

        gles1_driver_texture *tex = ctx->binded_texture();
        if (!tex || !tex->handle_value()) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        if ((xoffset < 0) || (yoffset < 0) || (xoffset + width > tex->size().x) || (yoffset + height > tex->size().y)) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        // On GLES3 this is true, unsure with GLES1, but still put here for our ease.
        if (tex->internal_format() != format) {
            LOG_ERROR(HLE_GLES1, "Internal format and format passed to texture update does not match!");
            controller.push_error(ctx, GL_INVALID_OPERATION);

            return;
        }

        bool need_reinstantiate = true;

        drivers::texture_format internal_format_driver;
        drivers::texture_format format_driver;
        drivers::texture_data_type dtype;
        drivers::channel_swizzles swizzles;

        get_data_type_to_upload(internal_format_driver, format_driver, dtype, swizzles, format, data_type);

        const std::size_t needed_size = calculate_possible_upload_size(eka2l1::vec2(width, height), format, data_type);
        ctx->cmd_builder_.update_texture(tex->handle_value(), reinterpret_cast<const char*>(data_pixels), needed_size,
            static_cast<std::uint8_t>(level), format_driver, dtype, eka2l1::vec3(xoffset, yoffset, 0), eka2l1::vec3(width, height, 0),
            0, ctx->unpack_alignment_);

        if (tex->auto_regenerate_mipmap()) {
            ctx->cmd_builder_.regenerate_mips(tex->handle_value());
        }
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

    BRIDGE_FUNC_LIBRARY(void, gl_active_texture_emu, std::uint32_t unit) {
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

        ctx->active_texture_unit_ = unit - GL_TEXTURE0_EMU;
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
    
    BRIDGE_FUNC_LIBRARY(void, gl_buffer_data_emu, std::uint32_t target, std::int32_t size, const void *data, std::uint32_t usage) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        gles1_driver_buffer *buffer = ctx->binded_buffer(target == GL_ARRAY_BUFFER_EMU);
        if (!buffer) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        if (size < 0) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        drivers::buffer_upload_hint upload_hint;

        switch (usage) {
        case GL_STATIC_DRAW_EMU:
            upload_hint = static_cast<drivers::buffer_upload_hint>(drivers::buffer_upload_static | drivers::buffer_upload_draw);
            break;

        case GL_DYNAMIC_DRAW_EMU:
            upload_hint = static_cast<drivers::buffer_upload_hint>(drivers::buffer_upload_dynamic | drivers::buffer_upload_draw);
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        bool need_reinstantiate = true;

        if (buffer->handle_value() == 0) {
            if (!ctx->buffer_pools_.empty()) {
                buffer->assign_handle(ctx->buffer_pools_.top());
                ctx->buffer_pools_.pop();
            } else {
                drivers::graphics_driver *drv = sys->get_graphics_driver();
                drivers::handle new_h = drivers::create_buffer(drv, data, size, upload_hint);

                if (!new_h) {
                    controller.push_error(ctx, GL_INVALID_OPERATION);
                    return;
                }

                need_reinstantiate = false;
                buffer->assign_handle(new_h);
            }
        }

        if (need_reinstantiate) {
            ctx->cmd_builder_.recreate_buffer(buffer->handle_value(), data, size, upload_hint);
        }

        buffer->assign_data_size(static_cast<std::uint32_t>(size));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_buffer_sub_data_emu, std::uint32_t target, std::int32_t offset, std::int32_t size, const void *data) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        gles1_driver_buffer *buffer = ctx->binded_buffer(target == GL_ARRAY_BUFFER_EMU);
        if (!buffer || !buffer->handle_value()) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        if ((size < 0) || (offset < 0) || ((offset + size) > static_cast<std::int32_t>(buffer->data_size()))) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        std::uint32_t size_upload_casted = static_cast<std::uint32_t>(size);
        ctx->cmd_builder_.update_buffer_data(buffer->handle_value(), static_cast<std::size_t>(offset), 1,
            &data, &size_upload_casted);
    }

    static bool gl_enum_to_drivers_data_format(const std::uint32_t param, drivers::data_format &res) {
        switch (param) {
        case GL_BYTE_EMU:
            res = drivers::data_format::sbyte;
            break;

        case GL_SHORT_EMU:
            res = drivers::data_format::sword;
            break;

        case GL_FLOAT_EMU:
            res = drivers::data_format::sfloat;
            break;

        case GL_FIXED_EMU:
            res = drivers::data_format::fixed;
            break;

        case GL_UNSIGNED_BYTE_EMU:
            res = drivers::data_format::byte;
            break;

        case GL_UNSIGNED_SHORT_EMU:
            res = drivers::data_format::word;
            break;

        default:
            return false;
        }

        return true;
    }

    static bool assign_vertex_attrib_gles1(gles1_vertex_attrib &attrib, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset, std::uint32_t buffer_obj) {
        drivers::data_format temp;
        if (!gl_enum_to_drivers_data_format(type, temp)) {
            return false;
        }

        attrib.data_type_ = type;
        attrib.offset_ = static_cast<std::uint32_t>(offset);
        attrib.size_ = size;
        attrib.stride_ = stride;
        attrib.buffer_obj_ = buffer_obj;

        return true;
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

        if (!assign_vertex_attrib_gles1(ctx->color_attrib_, size, type, stride, offset, ctx->binded_array_buffer_handle_)) {
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

        if (!assign_vertex_attrib_gles1(ctx->normal_attrib_, 3, type, stride, offset, ctx->binded_array_buffer_handle_)) {
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

        if (!assign_vertex_attrib_gles1(ctx->vertex_attrib_, size, type, stride, offset, ctx->binded_array_buffer_handle_)) {
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

        if (!assign_vertex_attrib_gles1(ctx->texture_units_[ctx->active_client_texture_unit_].coord_attrib_, size, type, stride, offset, ctx->binded_array_buffer_handle_)) {
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

    BRIDGE_FUNC_LIBRARY(void, gl_color_mask_emu, std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->color_mask_ = 0;
        if (red) ctx->color_mask_ |= 1;
        if (green) ctx->color_mask_ |= 2;
        if (blue) ctx->color_mask_ |= 4;
        if (alpha) ctx->color_mask_ |= 8;

        ctx->state_change_tracker_ |= egl_context_es1::STATE_CHANGED_COLOR_MASK;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_blend_func_emu, std::uint32_t source_factor, std::uint32_t dest_factor) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        drivers::blend_factor source_factor_driver;
        drivers::blend_factor dest_factor_driver;

        if (!convert_gl_factor_to_driver_enum(source_factor, source_factor_driver) || !convert_gl_factor_to_driver_enum(dest_factor, dest_factor_driver)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->source_blend_factor_ = source_factor_driver;
        ctx->dest_blend_factor_ = dest_factor_driver;

        ctx->state_change_tracker_ |= egl_context_es1::STATE_CHANGED_BLEND_FACTOR;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_stencil_mask_emu, std::uint32_t mask) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        if (ctx->stencil_mask_ != mask) {
            ctx->state_change_tracker_ |= egl_context_es1::STATE_CHANGED_STENCIL_MASK;
        }

        ctx->stencil_mask_ = mask;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_depth_mask_emu, std::uint32_t mask) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        if (ctx->depth_mask_ != mask) {
            ctx->state_change_tracker_ |= egl_context_es1::STATE_CHANGED_DEPTH_MASK;
        }

        ctx->depth_mask_ = mask;
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

        gles_texture_unit &unit = ctx->texture_units_[ctx->active_texture_unit_];
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
    
    BRIDGE_FUNC_LIBRARY(void, gl_depth_func_emu, std::uint32_t func) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        drivers::condition_func func_drv = drivers::condition_func::always;
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!cond_func_from_gl_enum(func, func_drv)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (ctx->depth_func_ != func) {
            ctx->state_change_tracker_ |= egl_context_es1::STATE_CHANGED_DEPTH_PASS_COND;
        }

        ctx->depth_func_ = func;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_stencil_func_emu, std::uint32_t func, std::int32_t ref, std::uint32_t mask) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        drivers::condition_func func_drv = drivers::condition_func::always;
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!cond_func_from_gl_enum(func, func_drv)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if ((ctx->stencil_func_ != func) || (ctx->stencil_func_ref_ != ref) || (ctx->stencil_func_mask_ != mask)) {
            ctx->state_change_tracker_ |= egl_context_es1::STATE_CHANGED_STENCIL_FUNC;
        }

        ctx->stencil_func_ = func;
        ctx->stencil_func_ref_ = ref;
        ctx->stencil_func_mask_ = mask;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_stencil_op_emu, std::uint32_t fail, std::uint32_t depth_fail, std::uint32_t depth_pass) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        drivers::stencil_action action_temp = drivers::stencil_action::keep;
        if (!stencil_action_from_gl_enum(fail, action_temp) || !stencil_action_from_gl_enum(depth_fail, action_temp) ||
            !stencil_action_from_gl_enum(depth_pass, action_temp)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if ((ctx->stencil_fail_action_ != fail) || (ctx->stencil_depth_fail_action_ != depth_fail) ||
            (ctx->stencil_depth_pass_action_ != depth_pass)) {
            ctx->state_change_tracker_ |= egl_context_es1::STATE_CHANGED_STENCIL_OP;
        }

        ctx->stencil_fail_action_ = fail;
        ctx->stencil_depth_fail_action_ = depth_fail;
        ctx->stencil_depth_pass_action_ = depth_pass;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_enable_emu, std::uint32_t cap) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (cap) {
        case GL_ALPHA_TEST_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_ALPHA_TEST;
            break;

        case GL_BLEND_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_BLEND_ENABLE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::blend, true);

            break;

        case GL_COLOR_LOGIC_OP_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_COLOR_LOGIC_OP_ENABLE;
            break;

        case GL_CLIP_PLANE0_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE0_ENABLE;
            break;
            
        case GL_CLIP_PLANE1_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE1_ENABLE;
            break;

        case GL_CLIP_PLANE2_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE2_ENABLE;
            break;

        case GL_CLIP_PLANE3_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE3_ENABLE;
            break;

        case GL_CLIP_PLANE4_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE4_ENABLE;
            break;

        case GL_CLIP_PLANE5_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_CLIP_PLANE5_ENABLE;
            break;

        case GL_CULL_FACE_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_CULL_FACE_ENABLE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::cull, true);

            break;

        case GL_COLOR_MATERIAL_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_COLOR_MATERIAL_ENABLE;
            break;

        case GL_DEPTH_TEST_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_DEPTH_TEST_ENABLE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::depth_test, true);

            break;

        case GL_STENCIL_TEST_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_STENCIL_TEST_ENABLE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::stencil_test, true);

            break;

        case GL_FOG_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_FOG_ENABLE;
            break;

        case GL_LIGHTING_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHTING_ENABLE;
            break;

        case GL_LIGHT0_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT0_ON;
            break;

        case GL_LIGHT1_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT1_ON;
            break;

        case GL_LIGHT2_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT2_ON;
            break;

        case GL_LIGHT3_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT3_ON;
            break;

        case GL_LIGHT4_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT4_ON;
            break;

        case GL_LIGHT5_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT5_ON;
            break;

        case GL_LIGHT6_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT6_ON;
            break;
        
        case GL_LIGHT7_EMU:
            ctx->fragment_statuses_ |= egl_context_es1::FRAGMENT_STATE_LIGHT7_ON;
            break;

        case GL_LINE_SMOOTH_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_LINE_SMOOTH;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::line_smooth, true);

            break;

        case GL_DITHER_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_DITHER;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::dither, true);

            break;

        case GL_SCISSOR_TEST_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_SCISSOR_ENABLE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::clipping, true);

            break;

        case GL_SAMPLE_COVERAGE_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_SAMPLE_COVERAGE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::sample_coverage, true);

            break;

        case GL_MULTISAMPLE_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_MULTISAMPLE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::multisample, true);

            break;

        case GL_SAMPLE_ALPHA_TO_COVERAGE_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_SAMPLE_ALPHA_TO_COVERAGE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::sample_alpha_to_coverage, true);

            break;

        case GL_SAMPLE_ALPHA_TO_ONE_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_SAMPLE_ALPHA_TO_ONE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::sample_alpha_to_one, true);

            break;

        case GL_TEXTURE_2D_EMU:
            ctx->texture_units_[ctx->active_texture_unit_].texturing_enabled_ = true;
            break;

        case GL_NORMALIZE_EMU:
            ctx->vertex_statuses_ |= egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_NORMALIZE;
            break;

        case GL_RESCALE_NORMAL_EMU:
            ctx->vertex_statuses_ |= egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_RESCALE;
            break;

        case GL_POLYGON_OFFSET_FILL_EMU:
            ctx->non_shader_statuses_ |= egl_context_es1::NON_SHADER_STATE_POLYGON_OFFSET_FILL;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::polygon_offset_fill, true);

            break;

        case GL_MATRIX_PALETTE_OES:
            ctx->vertex_statuses_ |= egl_context_es1::VERTEX_STATE_SKINNING_ENABLE;
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_disable_emu, std::uint32_t cap) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (cap) {
        case GL_ALPHA_TEST_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_ALPHA_TEST;
            break;

        case GL_BLEND_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_BLEND_ENABLE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::blend, false);

            break;

        case GL_COLOR_LOGIC_OP_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_COLOR_LOGIC_OP_ENABLE;
            break;

        case GL_CLIP_PLANE0_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE0_ENABLE;
            break;
            
        case GL_CLIP_PLANE1_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE1_ENABLE;
            break;

        case GL_CLIP_PLANE2_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE2_ENABLE;
            break;

        case GL_CLIP_PLANE3_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE3_ENABLE;
            break;

        case GL_CLIP_PLANE4_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE4_ENABLE;
            break;

        case GL_CLIP_PLANE5_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_CLIP_PLANE5_ENABLE;
            break;

        case GL_CULL_FACE_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_CULL_FACE_ENABLE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::cull, false);

            break;

        case GL_COLOR_MATERIAL_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_COLOR_MATERIAL_ENABLE;
            break;

        case GL_DEPTH_TEST_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_DEPTH_TEST_ENABLE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::depth_test, false);

            break;

        case GL_STENCIL_TEST_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_STENCIL_TEST_ENABLE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::stencil_test, false);

            break;

        case GL_FOG_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_FOG_ENABLE;
            break;

        case GL_LIGHTING_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHTING_ENABLE;
            break;

        case GL_LIGHT0_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT0_ON;
            break;

        case GL_LIGHT1_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT1_ON;
            break;

        case GL_LIGHT2_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT2_ON;
            break;

        case GL_LIGHT3_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT3_ON;
            break;

        case GL_LIGHT4_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT4_ON;
            break;

        case GL_LIGHT5_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT5_ON;
            break;

        case GL_LIGHT6_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT6_ON;
            break;
        
        case GL_LIGHT7_EMU:
            ctx->fragment_statuses_ &= ~egl_context_es1::FRAGMENT_STATE_LIGHT7_ON;
            break;

        case GL_LINE_SMOOTH_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_LINE_SMOOTH;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::line_smooth, false);

            break;

        case GL_DITHER_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_DITHER;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::dither, false);

            break;

        case GL_SCISSOR_TEST_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_SCISSOR_ENABLE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::clipping, false);

            break;

        case GL_SAMPLE_COVERAGE_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_SAMPLE_COVERAGE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::sample_coverage, false);

            break;

        case GL_MULTISAMPLE_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_MULTISAMPLE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::multisample, false);

            break;

        case GL_SAMPLE_ALPHA_TO_COVERAGE_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_SAMPLE_ALPHA_TO_COVERAGE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::sample_alpha_to_coverage, false);

            break;

        case GL_SAMPLE_ALPHA_TO_ONE_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_SAMPLE_ALPHA_TO_ONE;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::sample_alpha_to_one, false);
            break;

        case GL_TEXTURE_2D_EMU:
            ctx->texture_units_[ctx->active_texture_unit_].texturing_enabled_ = false;
            break;

        case GL_NORMALIZE_EMU:
            ctx->vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_NORMALIZE;
            break;

        case GL_RESCALE_NORMAL_EMU:
            ctx->vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_RESCALE;
            break;

        case GL_POLYGON_OFFSET_FILL_EMU:
            ctx->non_shader_statuses_ &= ~egl_context_es1::NON_SHADER_STATE_POLYGON_OFFSET_FILL;
            ctx->cmd_builder_.set_feature(drivers::graphics_feature::polygon_offset_fill, false);

            break;

        case GL_MATRIX_PALETTE_OES:
            ctx->vertex_statuses_ &= ~egl_context_es1::VERTEX_STATE_SKINNING_ENABLE;
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
    
    BRIDGE_FUNC_LIBRARY(void, gl_viewport_emu, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->viewport_bl_.top = eka2l1::vec2(x, y);
        ctx->viewport_bl_.size = eka2l1::vec2(width, height);

        ctx->state_change_tracker_ |= egl_context_es1::STATE_CHANGED_VIEWPORT_RECT;
    }

    static bool gl_enum_to_addressing_option(const std::uint32_t param, drivers::addressing_option &res) {
        switch (param) {
        case GL_REPEAT_EMU:
            res = drivers::addressing_option::repeat;
            break;

        case GL_CLAMP_TO_EDGE_EMU:
            res = drivers::addressing_option::clamp_to_edge;
            break;

        default:
            return false;
        }

        return true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_i_emu, std::uint32_t target, std::uint32_t pname, std::int32_t param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        gles1_driver_texture *tex = ctx->binded_texture();
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        drivers::graphics_driver *drv = sys->get_graphics_driver();

        if (!tex) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        if (target != GL_TEXTURE_2D_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        switch (pname) {
        case GL_TEXTURE_MAG_FILTER_EMU: {
            drivers::filter_option opt;
            switch (param) {
            case GL_LINEAR_EMU:
                opt = drivers::filter_option::linear;
                break;

            case GL_NEAREST_EMU:
                opt = drivers::filter_option::nearest;
                break;

            default:
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            tex->set_mag_filter(param);
            ctx->cmd_builder_.set_texture_filter(tex->handle_value(), false, opt);

            break;
        }

        case GL_TEXTURE_MIN_FILTER_EMU: {
            drivers::filter_option opt;
            switch (param) {
            case GL_LINEAR_EMU:
                opt = drivers::filter_option::linear;
                break;

            case GL_NEAREST_EMU:
                opt = drivers::filter_option::nearest;
                break;

            case GL_LINEAR_MIPMAP_LINEAR_EMU:
                opt = drivers::filter_option::linear_mipmap_linear;
                break;

            case GL_LINEAR_MIPMAP_NEAREST_EMU:
                opt = drivers::filter_option::linear_mipmap_nearest;
                break;

            case GL_NEAREST_MIPMAP_NEAREST_EMU:
                opt = drivers::filter_option::nearest_mipmap_nearest;
                break;

            case GL_NEAREST_MIPMAP_LINEAR_EMU:
                opt = drivers::filter_option::nearest_mipmap_linear;
                break;

            default:
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            tex->set_min_filter(param);
            ctx->cmd_builder_.set_texture_filter(tex->handle_value(), true, opt);

            break;
        }

        case GL_TEXTURE_WRAP_S_EMU: {
            drivers::addressing_option opt;
            if (!gl_enum_to_addressing_option(param, opt)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            tex->set_wrap_s(param);
            ctx->cmd_builder_.set_texture_addressing_mode(tex->handle_value(), drivers::addressing_direction::s,
                opt);

            break;
        }
        
        case GL_TEXTURE_WRAP_T_EMU: {
            drivers::addressing_option opt;
            if (!gl_enum_to_addressing_option(param, opt)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            tex->set_wrap_t(param);
            ctx->cmd_builder_.set_texture_addressing_mode(tex->handle_value(), drivers::addressing_direction::t,
                opt);

            break;
        }

        case GL_GENERATE_MIPMAP_EMU:
            tex->set_auto_regenerate_mipmap(param != 0);
            break;

        case GL_TEXTURE_MAX_ANISOTROPHY_EMU: {
            if (!drv->support_extension(drivers::graphics_driver_extension_anisotrophy_filtering)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            float max_max_ani = 1.0f;
            drv->query_extension_value(drivers::graphics_driver_extension_query_max_texture_max_anisotrophy, &max_max_ani);

            if (tex->max_anisotrophy() != param) {
                if ((param > max_max_ani) || (param < 1.0f)) {
                    controller.push_error(ctx, GL_INVALID_VALUE);
                    return;
                }

                ctx->cmd_builder_.set_texture_anisotrophy(tex->handle_value(), static_cast<float>(param));
                tex->set_max_anisotrophy(static_cast<float>(param));
            }

            break;
        }

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_f_emu, std::uint32_t target, std::uint32_t pname, float param) {
        if (pname == GL_TEXTURE_MAX_ANISOTROPHY_EMU) {
            egl_context_es1 *ctx = get_es1_active_context(sys);
            if (!ctx) {
                return;
            }

            gles1_driver_texture *tex = ctx->binded_texture();
            dispatcher *dp = sys->get_dispatcher();
            dispatch::egl_controller &controller = dp->get_egl_controller();
            drivers::graphics_driver *drv = sys->get_graphics_driver();

            if (!tex) {
                controller.push_error(ctx, GL_INVALID_OPERATION);
                return;
            }

            if (!drv->support_extension(drivers::graphics_driver_extension_anisotrophy_filtering)) {
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            float max_max_ani = 1.0f;
            drv->query_extension_value(drivers::graphics_driver_extension_query_max_texture_max_anisotrophy, &max_max_ani);

            if (tex->max_anisotrophy() != param) {
                if ((param > max_max_ani) || (param < 1.0f)) {
                    controller.push_error(ctx, GL_INVALID_VALUE);
                    return;
                }

                ctx->cmd_builder_.set_texture_anisotrophy(tex->handle_value(), param);
                tex->set_max_anisotrophy(param);
            }
        } else {
            gl_tex_parameter_i_emu(sys, target, pname, static_cast<std::int32_t>(param));
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_x_emu, std::uint32_t target, std::uint32_t pname, gl_fixed param) {
        if (pname == GL_TEXTURE_MAX_ANISOTROPHY_EMU) {
            gl_tex_parameter_f_emu(sys, target, pname, FIXED_32_TO_FLOAT(param));
            return;
        }

        gl_tex_parameter_i_emu(sys, target, pname, static_cast<std::int32_t>(param));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_iv_emu, std::uint32_t target, std::uint32_t pname, std::int32_t* param) {
        gl_tex_parameter_i_emu(sys, target, pname, *param);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_fv_emu, std::uint32_t target, std::uint32_t pname, float* param) {
        gl_tex_parameter_i_emu(sys, target, pname, static_cast<std::int32_t>(*param));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_xv_emu, std::uint32_t target, std::uint32_t pname, gl_fixed* param) {
        gl_tex_parameter_i_emu(sys, target, pname, static_cast<std::int32_t>(*param));
    }

    static bool prepare_shader_program_for_draw(dispatch::egl_controller &controller, egl_context_es1 *ctx, const std::uint32_t active_texs) {
        dispatch::gles_texture_env_info info_arr[GLES1_EMU_MAX_TEXTURE_COUNT];
        for (std::int32_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
            info_arr[i] = ctx->texture_units_[i].env_info_;
        }

        dispatch::gles1_shader_variables_info *var_info = nullptr;

        drivers::handle program = controller.get_es1_shaderman().retrieve_program(ctx->vertex_statuses_, ctx->fragment_statuses_, active_texs,
            info_arr, var_info);

        if (!program) {
            LOG_ERROR(HLE_DISPATCHER, "Problem while retrieveing GLES1 shader program!");
            return false;
        }

        ctx->cmd_builder_.use_program(program);

        if (var_info) {
            // Not binded by uniform buffer/constant buffer
            if (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_SKINNING_ENABLE) {
                std::vector<std::uint8_t> palette_mats_data(64 * GLES1_EMU_MAX_PALETTE_MATRICES);
                for (std::size_t i = 0; i < GLES1_EMU_MAX_PALETTE_MATRICES; i++) {
                    memcpy(palette_mats_data.data() + i * 64, glm::value_ptr(ctx->palette_mats_[i]), 64);
                }
                ctx->cmd_builder_.set_dynamic_uniform(var_info->palette_mat_loc_, drivers::shader_set_var_type::mat4,
                    palette_mats_data.data(), palette_mats_data.size());
            } else {
                ctx->cmd_builder_.set_dynamic_uniform(var_info->view_model_mat_loc_, drivers::shader_set_var_type::mat4,
                    glm::value_ptr(ctx->model_view_mat_stack_.top()), 64);
            }

            ctx->cmd_builder_.set_dynamic_uniform(var_info->proj_mat_loc_, drivers::shader_set_var_type::mat4,
                glm::value_ptr(ctx->proj_mat_stack_.top()), 64);

            if ((ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY) == 0) {
                ctx->cmd_builder_.set_dynamic_uniform(var_info->color_loc_, drivers::shader_set_var_type::vec4,
                    ctx->color_uniforms_, 16);
            }

            if ((ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY) == 0) {
                ctx->cmd_builder_.set_dynamic_uniform(var_info->normal_loc_, drivers::shader_set_var_type::vec3,
                    ctx->normal_uniforms_, 12);
            }

            std::uint64_t coordarray_mask = egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD0_ARRAY;

            for (std::uint32_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++, coordarray_mask <<= 1) {
                if (active_texs & (1 << i)) {
                    if ((ctx->vertex_statuses_ & coordarray_mask) == 0)
                        ctx->cmd_builder_.set_dynamic_uniform(var_info->texcoord_loc_[i], drivers::shader_set_var_type::vec4,
                            ctx->texture_units_[i].coord_uniforms_, 16);

                    ctx->cmd_builder_.set_dynamic_uniform(var_info->texture_mat_loc_[i], drivers::shader_set_var_type::mat4,
                        glm::value_ptr(ctx->texture_units_[i].texture_mat_stack_.top()), 64);

                    ctx->cmd_builder_.set_texture_for_shader(i, var_info->texview_loc_[i], drivers::shader_module_type::fragment);
                    ctx->cmd_builder_.set_dynamic_uniform(var_info->texenv_color_loc_[i], drivers::shader_set_var_type::vec4,
                        ctx->texture_units_[i].env_colors_, 16);
                }
            }
            
            for (std::uint8_t i = 0; i < GLES1_EMU_MAX_CLIP_PLANE; i++) {
                if (ctx->fragment_statuses_ & (1 << (egl_context_es1::FRAGMENT_STATE_CLIP_PLANE_BIT_POS + i))) {
                    ctx->cmd_builder_.set_dynamic_uniform(var_info->clip_plane_loc_[i], drivers::shader_set_var_type::vec4,
                        ctx->clip_planes_transformed_[i], 16);
                }
            }

            ctx->cmd_builder_.set_dynamic_uniform(var_info->material_ambient_loc_, drivers::shader_set_var_type::vec4,
                ctx->material_ambient_, 16);

            ctx->cmd_builder_.set_dynamic_uniform(var_info->material_diffuse_loc_, drivers::shader_set_var_type::vec4,
                ctx->material_diffuse_, 16);
            
            ctx->cmd_builder_.set_dynamic_uniform(var_info->material_specular_loc_, drivers::shader_set_var_type::vec4,
                ctx->material_specular_, 16);

            ctx->cmd_builder_.set_dynamic_uniform(var_info->material_emission_loc_, drivers::shader_set_var_type::vec4,
                ctx->material_emission_, 16);

            ctx->cmd_builder_.set_dynamic_uniform(var_info->material_shininess_loc_, drivers::shader_set_var_type::real,
                &ctx->material_shininess_, 4);

            ctx->cmd_builder_.set_dynamic_uniform(var_info->global_ambient_loc_, drivers::shader_set_var_type::vec4,
                ctx->global_ambient_, 16);

            if (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_ALPHA_TEST) {
                ctx->cmd_builder_.set_dynamic_uniform(var_info->alpha_test_ref_loc_, drivers::shader_set_var_type::real,
                    &ctx->alpha_test_ref_, 4);
            }

            if (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_FOG_ENABLE) {
                ctx->cmd_builder_.set_dynamic_uniform(var_info->fog_color_loc_, drivers::shader_set_var_type::vec4,
                    ctx->fog_color_, 16);

                std::uint64_t fog_mode = (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_FOG_MODE_MASK);

                if (fog_mode == egl_context_es1::FRAGMENT_STATE_FOG_MODE_LINEAR) {
                    ctx->cmd_builder_.set_dynamic_uniform(var_info->fog_start_loc_, drivers::shader_set_var_type::real,
                        &ctx->fog_start_, 4);
                    ctx->cmd_builder_.set_dynamic_uniform(var_info->fog_end_loc_, drivers::shader_set_var_type::real,
                        &ctx->fog_end_, 4);
                } else {
                    ctx->cmd_builder_.set_dynamic_uniform(var_info->fog_density_loc_, drivers::shader_set_var_type::real,
                        &ctx->fog_density_, 4);
                }
            }

            if (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHTING_ENABLE) {
                for (std::uint32_t i = 0, mask = egl_context_es1::FRAGMENT_STATE_LIGHT0_ON; i < GLES1_EMU_MAX_LIGHT; i++, mask <<= 1) {
                    if (ctx->fragment_statuses_ & mask) {
                        ctx->cmd_builder_.set_dynamic_uniform(var_info->light_dir_or_pos_loc_[i], drivers::shader_set_var_type::vec4,
                            ctx->lights_[i].position_or_dir_transformed_, 16);
                        ctx->cmd_builder_.set_dynamic_uniform(var_info->light_ambient_loc_[i], drivers::shader_set_var_type::vec4,
                            ctx->lights_[i].ambient_, 16);
                        ctx->cmd_builder_.set_dynamic_uniform(var_info->light_diffuse_loc_[i], drivers::shader_set_var_type::vec4,
                            ctx->lights_[i].diffuse_, 16);
                        ctx->cmd_builder_.set_dynamic_uniform(var_info->light_specular_loc_[i], drivers::shader_set_var_type::vec4,
                            ctx->lights_[i].specular_, 16);
                        ctx->cmd_builder_.set_dynamic_uniform(var_info->light_spot_dir_loc_[i], drivers::shader_set_var_type::vec3,
                            ctx->lights_[i].spot_dir_transformed_, 12);
                        ctx->cmd_builder_.set_dynamic_uniform(var_info->light_spot_cutoff_loc_[i], drivers::shader_set_var_type::real,
                            &ctx->lights_[i].spot_cutoff_, 4);
                        ctx->cmd_builder_.set_dynamic_uniform(var_info->light_spot_exponent_loc_[i], drivers::shader_set_var_type::real,
                            &ctx->lights_[i].spot_exponent_, 4);
                        ctx->cmd_builder_.set_dynamic_uniform(var_info->light_attenuatation_vec_loc_[i], drivers::shader_set_var_type::vec3,
                            ctx->lights_[i].attenuatation_, 12);
                    }
                }
            }
        } else {
            LOG_WARN(HLE_DISPATCHER, "Shader variables in buffer not yet supported!");
        }

        return true;
    }

    static void prepare_vertex_buffer_and_descriptors(egl_context_es1 *ctx, drivers::graphics_driver *drv, kernel::process *crr_process, const std::int32_t first_index, const std::uint32_t vcount, const std::uint32_t active_texs) {
        if (ctx->attrib_changed_ || (ctx->previous_first_index_ != first_index)) {
            std::vector<drivers::handle> vertex_buffers_alloc;
            bool can_turn_off_change = true;

            auto retrieve_vertex_buffer_slot = [&](gles1_vertex_attrib attrib, std::uint32_t &res, int &offset) -> bool {
                drivers::handle buffer_handle_drv = 0;
                if (attrib.buffer_obj_ == 0) {
                    std::uint8_t *data_raw = eka2l1::ptr<std::uint8_t>(attrib.offset_).get(crr_process);
                    if (!data_raw) {
                        LOG_ERROR(HLE_DISPATCHER, "Unable to retrieve raw pointer of non-buffer binded attribute!");
                        return false;
                    }

                    if (!ctx->vertex_buffer_pusher_.is_initialized()) {
                        ctx->vertex_buffer_pusher_.initialize(drv, common::MB(4));
                    }

                    std::size_t offset_big = 0;
                    buffer_handle_drv = ctx->vertex_buffer_pusher_.push_buffer(data_raw, attrib, first_index, vcount, offset_big);

                    offset = static_cast<int>(offset_big);

                    if (buffer_handle_drv == 0) {
                        // Buffers are full, need flushing all
                        ctx->flush_to_driver(drv);
                        buffer_handle_drv = ctx->vertex_buffer_pusher_.push_buffer(data_raw, attrib, first_index, vcount, offset_big);
                    }

                    if (can_turn_off_change) {
                        can_turn_off_change = false;
                    }
                } else {
                    offset = static_cast<int>(attrib.offset_);
                    if (first_index) {
                        offset += first_index * get_gl_attrib_stride(attrib);
                    }
                    auto *buffer_inst_ptr = ctx->objects_.get(attrib.buffer_obj_);
                    if (!buffer_inst_ptr || ((*buffer_inst_ptr)->object_type() != GLES1_OBJECT_BUFFER)) {
                        return false;
                    }

                    gles1_driver_buffer *buffer = reinterpret_cast<gles1_driver_buffer*>((*buffer_inst_ptr).get());
                    buffer_handle_drv = buffer->handle_value();
                }

                auto ite = std::find(vertex_buffers_alloc.begin(), vertex_buffers_alloc.end(), buffer_handle_drv);
                if (ite != vertex_buffers_alloc.end()) {
                    res = static_cast<std::uint32_t>(std::distance(vertex_buffers_alloc.begin(), ite));
                    return true;
                }

                vertex_buffers_alloc.push_back(buffer_handle_drv);
                res = static_cast<std::uint32_t>(vertex_buffers_alloc.size() - 1);

                return true;
            };

            // Remade and attach descriptors
            std::vector<drivers::input_descriptor> descs;
            drivers::data_format temp_format;

            drivers::input_descriptor temp_desc;
            if (!retrieve_vertex_buffer_slot(ctx->vertex_attrib_, temp_desc.buffer_slot, temp_desc.offset)) {
                LOG_WARN(HLE_DISPATCHER, "Vertex attribute not bound to a valid buffer, draw call skipping!");
                return;
            }

            temp_desc.location = 0;
            temp_desc.stride = ctx->vertex_attrib_.stride_;

            gl_enum_to_drivers_data_format(ctx->vertex_attrib_.data_type_, temp_format);
            temp_desc.set_format(ctx->vertex_attrib_.size_, temp_format);

            if (ctx->vertex_attrib_.data_type_ == GL_FIXED_EMU) {
                temp_desc.set_normalized(true);
            }

            descs.push_back(temp_desc);

            if (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY) {
                if (retrieve_vertex_buffer_slot(ctx->color_attrib_, temp_desc.buffer_slot, temp_desc.offset)) {
                    temp_desc.location = 1;
                    temp_desc.stride = ctx->color_attrib_.stride_;
                    temp_desc.set_normalized(true);

                    gl_enum_to_drivers_data_format(ctx->color_attrib_.data_type_, temp_format);
                    temp_desc.set_format(ctx->color_attrib_.size_, temp_format);

                    descs.push_back(temp_desc);
                }
            }

            temp_desc.set_normalized(false);

            if (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY) {
                if (retrieve_vertex_buffer_slot(ctx->normal_attrib_, temp_desc.buffer_slot, temp_desc.offset)) {
                    temp_desc.location = 2;
                    temp_desc.stride = ctx->normal_attrib_.stride_;

                    gl_enum_to_drivers_data_format(ctx->normal_attrib_.data_type_, temp_format);
                    temp_desc.set_format(ctx->normal_attrib_.size_, temp_format);
                    temp_desc.set_normalized(true);

                    descs.push_back(temp_desc);
                }
            }

            temp_desc.set_normalized(false);

            if (active_texs) {
                for (std::int32_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
                    if ((active_texs & (1 << i)) && (ctx->vertex_statuses_ & (1 << (egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD_ARRAY_POS + static_cast<std::uint8_t>(i))))
                        && retrieve_vertex_buffer_slot(ctx->texture_units_[i].coord_attrib_, temp_desc.buffer_slot, temp_desc.offset)) {
                        temp_desc.location = 3 + i;
                        temp_desc.stride = ctx->texture_units_[i].coord_attrib_.stride_;

                        gl_enum_to_drivers_data_format(ctx->texture_units_[i].coord_attrib_.data_type_, temp_format);
                        temp_desc.set_format(ctx->texture_units_[i].coord_attrib_.size_, temp_format);

                        descs.push_back(temp_desc);
                    }
                }
            }

            temp_desc.set_normalized(false);

            if (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_SKINNING_ENABLE) {
                if (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_MATRIX_INDEX_ARRAY) {
                    if (retrieve_vertex_buffer_slot(ctx->matrix_index_attrib_, temp_desc.buffer_slot, temp_desc.offset)) {
                        temp_desc.location = 6;
                        temp_desc.stride = ctx->matrix_index_attrib_.stride_;
                        temp_desc.set_format(ctx->matrix_index_attrib_.size_, drivers::data_format::byte);

                        descs.push_back(temp_desc);
                    }
                }

                if (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_WEIGHT_ARRAY) {
                    if (retrieve_vertex_buffer_slot(ctx->weight_attrib_, temp_desc.buffer_slot, temp_desc.offset)) {
                        temp_desc.location = 7;
                        temp_desc.stride = ctx->weight_attrib_.stride_;

                        gl_enum_to_drivers_data_format(ctx->weight_attrib_.data_type_, temp_format);
                        temp_desc.set_format(ctx->weight_attrib_.size_, temp_format);

                        if (temp_format == drivers::data_format::fixed) {
                            temp_desc.set_normalized(true);
                        }

                        descs.push_back(temp_desc);
                    }
                }
            }

            if (!ctx->input_desc_) {
                ctx->input_desc_ = drivers::create_input_descriptors(drv, descs.data(), static_cast<std::uint32_t>(descs.size()));
            } else {
                ctx->cmd_builder_.update_input_descriptors(ctx->input_desc_, descs.data(), static_cast<std::uint32_t>(descs.size()));
            }

            ctx->cmd_builder_.set_vertex_buffers(vertex_buffers_alloc.data(), 0, static_cast<std::uint32_t>(vertex_buffers_alloc.size()));

            if (can_turn_off_change)
                ctx->attrib_changed_ = false;

            ctx->previous_first_index_ = first_index;
        }

        ctx->cmd_builder_.bind_input_descriptors(ctx->input_desc_);
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

    static bool prepare_gles1_draw(egl_context_es1 *ctx, drivers::graphics_driver *drv, kernel::process *crr_process, const std::int32_t first_index, const std::uint32_t vcount, dispatch::egl_controller &controller) {
        if ((ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_VERTEX_ARRAY) == 0) {
            // No drawing needed?
            return true;
        }

        std::uint32_t active_textures_bitarr = retrieve_active_textures_bitarr(ctx);
        prepare_vertex_buffer_and_descriptors(ctx, drv, crr_process, first_index, vcount, active_textures_bitarr);

        ctx->flush_state_changes();

        // Active textures
        for (std::int32_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
            if (active_textures_bitarr & (1 << i)) {
                auto *obj = ctx->objects_.get(ctx->texture_units_[i].binded_texture_handle_);

                if (obj)
                    ctx->cmd_builder_.bind_texture((*obj)->handle_value(), i);
            }
        }

        if (!prepare_shader_program_for_draw(controller, ctx, active_textures_bitarr)) {
            return false;
        }

        return true;
    }

    static bool convert_gl_enum_to_primitive_mode(const std::uint32_t mode, drivers::graphics_primitive_mode &res) {
        switch (mode) {
        case GL_POINTS_EMU:
            res = drivers::graphics_primitive_mode::points;
            break;

        case GL_LINES_EMU:
            res = drivers::graphics_primitive_mode::lines;
            break;

        case GL_LINE_LOOP_EMU:
            res = drivers::graphics_primitive_mode::line_loop;
            break;

        case GL_LINE_STRIP_EMU:
            res = drivers::graphics_primitive_mode::line_strip;
            break;

        case GL_TRIANGLES_EMU:
            res = drivers::graphics_primitive_mode::triangles;
            break;

        case GL_TRIANGLE_STRIP_EMU:
            res = drivers::graphics_primitive_mode::triangle_strip;
            break;

        case GL_TRIANGLE_FAN_EMU:
            res = drivers::graphics_primitive_mode::triangle_fan;
            break;

        default:
            return false;
        }

        return true;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_draw_arrays_emu, std::uint32_t mode, std::int32_t first_index, std::int32_t count) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        drivers::graphics_driver *drv = sys->get_graphics_driver();

        drivers::graphics_primitive_mode prim_mode_drv;
        if (!convert_gl_enum_to_primitive_mode(mode, prim_mode_drv)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (count < 0) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        if (!prepare_gles1_draw(ctx, drv, sys->get_kernel_system()->crr_process(), first_index, count, controller)) {
            LOG_ERROR(HLE_DISPATCHER, "Error while preparing GLES1 draw. This should not happen!");
            return;
        }

        ctx->cmd_builder_.draw_arrays(prim_mode_drv, 0, count, false);
 
        if (ctx->cmd_builder_.need_flush()) {
            ctx->flush_to_driver(drv);
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_draw_elements_emu, std::uint32_t mode, std::int32_t count, const std::uint32_t index_type,
        std::uint32_t indices_ptr) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        drivers::graphics_driver *drv = sys->get_graphics_driver();

        drivers::graphics_primitive_mode prim_mode_drv;
        if (!convert_gl_enum_to_primitive_mode(mode, prim_mode_drv)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (count < 0) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        drivers::data_format index_format_drv;
        std::size_t size_ibuffer = 0;

        switch (index_type) {
        case GL_UNSIGNED_BYTE_EMU:
            index_format_drv = drivers::data_format::byte;
            size_ibuffer = static_cast<std::size_t>(count);
            break;

        case GL_UNSIGNED_SHORT_EMU:
            index_format_drv = drivers::data_format::word;
            size_ibuffer = static_cast<std::size_t>(count * 2);
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        kernel_system *kern = sys->get_kernel_system();
        const std::uint8_t *indicies_data_raw = nullptr;

        std::int32_t total_vert = count;

        if (ctx->binded_element_array_buffer_handle_ == 0) {
            indicies_data_raw = reinterpret_cast<const std::uint8_t*>(kern->crr_process()->get_ptr_on_addr_space(indices_ptr));
 
            if (indicies_data_raw) {
                std::int32_t max_vert_index = 0;
                for (std::int32_t i = 0; i < count; i++) {
                    std::int32_t index = 0;
                    if (index_type == GL_UNSIGNED_BYTE_EMU) {
                        index = static_cast<std::int32_t>((reinterpret_cast<const std::uint8_t*>(indicies_data_raw))[i]);
                    } else {
                        index = static_cast<std::int32_t>((reinterpret_cast<const std::uint16_t*>(indicies_data_raw))[i]);
                    }

                    max_vert_index = common::max(index, max_vert_index);
                }

                total_vert = max_vert_index + 1;
            }
        }

        gles1_driver_buffer *binded_elem_buffer_managed = ctx->binded_buffer(false);

        if (!binded_elem_buffer_managed) {
            // Upload it to a temp buffer (sadly!)
            if (!indicies_data_raw) {
                LOG_ERROR(HLE_DISPATCHER, "Interpreting indices pointer as a real pointer, but invalid!");
                controller.push_error(ctx, GL_INVALID_OPERATION);

                return;
            }

            if (!ctx->index_buffer_pusher_.is_initialized()) {
                ctx->index_buffer_pusher_.initialize(drv, common::MB(2));
            }

            std::size_t offset_bytes = 0;
            drivers::handle to_bind = ctx->index_buffer_pusher_.push_buffer(indicies_data_raw, size_ibuffer, offset_bytes);
            if (to_bind == 0) {
                ctx->flush_to_driver(drv);
                to_bind = ctx->index_buffer_pusher_.push_buffer(indicies_data_raw, size_ibuffer, offset_bytes);
            }

            ctx->cmd_builder_.set_index_buffer(to_bind);
            indices_ptr = static_cast<std::uint32_t>(offset_bytes);
        } else {
            ctx->cmd_builder_.set_index_buffer(binded_elem_buffer_managed->handle_value());
        }

        if (!prepare_gles1_draw(ctx, drv, sys->get_kernel_system()->crr_process(), 0, total_vert, controller)) {
            LOG_ERROR(HLE_DISPATCHER, "Error while preparing GLES1 draw. This should not happen!");
            return;
        }

        ctx->cmd_builder_.draw_indexed(prim_mode_drv, count, index_format_drv, static_cast<int>(indices_ptr), 0);

        if (ctx->cmd_builder_.need_flush()) {
            ctx->flush_to_driver(drv);
        }
    }

    BRIDGE_FUNC_LIBRARY(address, gl_get_string_emu, std::uint32_t pname) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return 0;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((pname != GL_EXTENSIONS) && (pname != GL_VENDOR) && (pname != GL_RENDERER) && (pname != GL_VERSION)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return 0;
        }

        address res = dp->retrieve_static_string(pname);
        if (res == 0) {
            controller.push_error(ctx, GL_INVALID_ENUM);
        }
        return res;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_hint_emu) {
        // Empty intentionally.
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_line_width_emu, float width) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (width < 0) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        ctx->line_width_ = width;
        ctx->state_change_tracker_ |= egl_context_es1::STATE_CHANGED_LINE_WIDTH;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_line_widthx_emu, gl_fixed width) {
        gl_line_width_emu(sys, FIXED_32_TO_FLOAT(width));
    }

    static std::uint32_t driver_blend_factor_to_gl_enum(const drivers::blend_factor factor) {
        switch (factor) {
        case drivers::blend_factor::current_alpha:
            return GL_DST_ALPHA_EMU;

        case drivers::blend_factor::current_color:
            return GL_DST_COLOR;

        case drivers::blend_factor::frag_out_alpha:
            return GL_SRC_ALPHA_EMU;

        case drivers::blend_factor::frag_out_alpha_saturate:
            return GL_SRC_ALPHA_SATURATE_EMU;

        case drivers::blend_factor::frag_out_color:
            return GL_SRC_COLOR_EMU;

        case drivers::blend_factor::one:
            return GL_ONE_EMU;

        case drivers::blend_factor::one_minus_current_alpha:
            return GL_ONE_MINUS_DST_ALPHA_EMU;

        case drivers::blend_factor::one_minus_current_color:
            return GL_ONE_MINUS_DST_COLOR;

        case drivers::blend_factor::one_minus_frag_out_alpha:
            return GL_ONE_MINUS_SRC_ALPHA_EMU;

        case drivers::blend_factor::one_minus_frag_out_color:
            return GL_ONE_MINUS_SRC_COLOR_EMU;

        default:
            break;
        }

        return GL_ONE_EMU;
    }

    // NOTE: boolean cast not handled!
    template <typename T>
    void gl_get_implv_emu(eka2l1::system *sys, std::uint32_t pname, T *params, std::uint32_t scale_factor) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        drivers::graphics_driver *drv = sys->get_graphics_driver();
        
        if (!params) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        switch (pname) {
        case GL_ACTIVE_TEXTURE_EMU:
            *params = static_cast<T>(ctx->active_texture_unit_ + GL_TEXTURE0_EMU);
            break;

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

        case GL_ARRAY_BUFFER_BINDING_EMU:
            *params = static_cast<T>(ctx->binded_array_buffer_handle_);
            break;

        case GL_BLEND_EMU:
            if (ctx->fragment_statuses_ & egl_context_es1::NON_SHADER_STATE_BLEND_ENABLE)
                *params = 1;
            else
                *params = 0;

            break;

        case GL_BLEND_DST_EMU:
            *params = static_cast<T>(driver_blend_factor_to_gl_enum(ctx->dest_blend_factor_));
            break;

        case GL_BLEND_SRC_EMU:
            *params = static_cast<T>(driver_blend_factor_to_gl_enum(ctx->source_blend_factor_));
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

        case GL_COLOR_CLEAR_VALUE_EMU:
            params[0] = static_cast<T>(ctx->clear_color_[0] * scale_factor);
            params[1] = static_cast<T>(ctx->clear_color_[1] * scale_factor);
            params[2] = static_cast<T>(ctx->clear_color_[2] * scale_factor);
            params[3] = static_cast<T>(ctx->clear_color_[3] * scale_factor);
            break;

        case GL_COLOR_LOGIC_OP_EMU:
            *params = static_cast<T>(ctx->non_shader_statuses_ & egl_context_es1::NON_SHADER_STATE_COLOR_LOGIC_OP_ENABLE) ? 1 : 0;
            break;

        case GL_COLOR_MATERIAL_EMU:
            *params = static_cast<T>(ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_COLOR_MATERIAL_ENABLE) ? 1 : 0;
            break;

        case GL_COLOR_WRITEMASK_EMU:
            params[0] = static_cast<T>((ctx->color_mask_ & 1) ? 1 : 0);
            params[1] = static_cast<T>((ctx->color_mask_ & 2) ? 1 : 0);
            params[2] = static_cast<T>((ctx->color_mask_ & 4) ? 1 : 0);
            params[3] = static_cast<T>((ctx->color_mask_ & 8) ? 1 : 0);

            break;

        case GL_CULL_FACE_EMU:
            *params = static_cast<T>(ctx->non_shader_statuses_ & egl_context_es1::NON_SHADER_STATE_CULL_FACE_ENABLE);
            break;

        case GL_CULL_FACE_MODE_EMU:
            *params = static_cast<T>((ctx->active_cull_face_ == drivers::rendering_face::front) ? GL_FRONT_EMU : GL_BACK_EMU);
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

        case GL_DEPTH_CLEAR_EMU:
            *params = static_cast<T>(ctx->clear_depth_ * scale_factor);
            break;

        case GL_NORMAL_ARRAY_EMU:
            *params = (static_cast<T>(ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY)) ? 1 : 0;
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

        case GL_MAX_TEXTURE_SIZE_EMU:
            *params = GLES1_EMU_MAX_TEXTURE_SIZE;
            break;

        case GL_MAX_TEXTURE_UNITS_EMU:
            *params = GLES1_EMU_MAX_TEXTURE_COUNT;
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

        case GL_MAX_VIEWPORT_DIMS_EMU:
            // We clamp on this
            *params = static_cast<T>(GLES1_EMU_MAX_TEXTURE_SIZE);
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

        case GL_SMOOTH_LINE_WIDTH_RANGE_EMU:
            params[0] = static_cast<T>(1);
            params[1] = static_cast<T>(1);
            break;

        case GL_POINT_SIZE_MIN_EMU:
        case GL_POINT_SIZE_MAX_EMU:
            *params = static_cast<T>(1);
            break;

        case GL_MAX_TEXTURE_MAX_ANISOTROPHY_EMU: {
            float query_val = 1.0f;
            if (!drv->support_extension(drivers::graphics_driver_extension_anisotrophy_filtering)) {    
                controller.push_error(ctx, GL_INVALID_ENUM);
                return;
            }

            drv->query_extension_value(drivers::graphics_driver_extension_query_max_texture_max_anisotrophy, &query_val);
            *params = static_cast<T>(query_val);

            break;
        }

        case GL_UNPACK_ALIGNMENT_EMU:
            *params = static_cast<T>(ctx->unpack_alignment_);
            break;

        case GL_PACK_ALIGNMENT_EMU:
            *params = static_cast<T>(ctx->pack_alignment_);
            break;

        case GL_ELEMENT_ARRAY_BUFFER_BINDING_EMU:
            *params = static_cast<T>(ctx->binded_element_array_buffer_handle_);
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
            LOG_TRACE(HLE_DISPATCHER, "Unhandled integer attribute 0x{:X}!", pname);
            controller.push_error(ctx, GL_INVALID_ENUM);

            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_get_floatv_emu, std::uint32_t pname, float *params) {
        gl_get_implv_emu<float>(sys, pname, params, 1);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_get_integerv_emu, std::uint32_t pname, std::uint32_t *params) {
        gl_get_implv_emu<std::uint32_t>(sys, pname, params, 255);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_get_fixedv_emu, std::uint32_t pname, gl_fixed *params) {
        gl_get_implv_emu<gl_fixed>(sys, pname, params, 65536);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_get_booleanv_emu, std::uint32_t pname, std::int32_t *params) {
        gl_get_implv_emu<std::int32_t>(sys, pname, params, 255);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_finish_emu) {
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_polygon_offset_emu, float factors, float units) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->polygon_offset_factor_ = factors;
        ctx->polygon_offset_units_ = units;

        ctx->state_change_tracker_ |= egl_context_es1::STATE_CHANGED_DEPTH_BIAS;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_polygon_offsetx_emu, gl_fixed factors, gl_fixed units) {
        gl_polygon_offset_emu(sys, FIXED_32_TO_FLOAT(factors), FIXED_32_TO_FLOAT(units));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_pixel_storei_emu, std::uint32_t pname, std::int32_t param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((param != 1) && (param != 2) && (param != 4) && (param != 8)) {
            controller.push_error(ctx, GL_INVALID_VALUE);
            return;
        }

        switch (pname) {
        case GL_PACK_ALIGNMENT_EMU:
            ctx->pack_alignment_ = static_cast<std::uint32_t>(param);
            break;

        case GL_UNPACK_ALIGNMENT_EMU:
            ctx->unpack_alignment_ = static_cast<std::uint32_t>(param);
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_depth_rangef_emu, float near, float far) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }

        if ((ctx->depth_range_max_ != far) || (ctx->depth_range_min_ != near)) {
            ctx->state_change_tracker_ |= egl_context_es1::STATE_CHANGED_DEPTH_RANGE;
        }

        ctx->depth_range_min_ = near;
        ctx->depth_range_max_ = far;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_depth_rangex_emu, gl_fixed near, gl_fixed far) {
        gl_depth_rangef_emu(sys, FIXED_32_TO_FLOAT(near), FIXED_32_TO_FLOAT(far));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_get_tex_parameter_iv_emu, std::uint32_t target, std::uint32_t pname, std::uint32_t *param) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_TEXTURE_2D_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        gles1_driver_texture *tex = ctx->binded_texture();
        if (!tex) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        switch (pname) {
        case GL_TEXTURE_MIN_FILTER_EMU:
            *param = tex->get_min_filter();
            break;

        case GL_TEXTURE_MAG_FILTER_EMU:
            *param = tex->get_mag_filter();
            break;

        case GL_TEXTURE_WRAP_S_EMU:
            *param = tex->get_wrap_s();
            break;

        case GL_TEXTURE_WRAP_T_EMU:
            *param = tex->get_wrap_t();
            break;

        case GL_GENERATE_MIPMAP_EMU:
            *param = (tex->auto_regenerate_mipmap() ? 1 : 0);
            break;

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_get_tex_parameter_fv_emu, std::uint32_t target, std::uint32_t pname, float *param) {
        std::uint32_t value_real = 0;
        gl_get_tex_parameter_iv_emu(sys, target, pname, &value_real);

        *param = static_cast<float>(value_real);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_get_tex_parameter_xv_emu, std::uint32_t target, std::uint32_t pname, gl_fixed *param) {
        std::uint32_t value_real = 0;
        gl_get_tex_parameter_iv_emu(sys, target, pname, &value_real);

        *param = static_cast<gl_fixed>(value_real);
    }

    BRIDGE_FUNC_LIBRARY(std::int32_t, gl_is_enabled_emu, std::uint32_t cap) {
        egl_context_es1 *ctx = get_es1_active_context(sys);
        if (!ctx) {
            return 0;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        switch (cap) {
        case GL_ALPHA_TEST_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_ALPHA_TEST) ? 1 : 0;

        case GL_BLEND_EMU:
            return (ctx->non_shader_statuses_ & egl_context_es1::NON_SHADER_STATE_BLEND_ENABLE) ? 1 : 0;

        case GL_COLOR_LOGIC_OP_EMU:
            return (ctx->non_shader_statuses_ & egl_context_es1::NON_SHADER_STATE_COLOR_LOGIC_OP_ENABLE) ? 1 : 0;

        case GL_CLIP_PLANE0_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_CLIP_PLANE0_ENABLE) ? 1 : 0;

        case GL_CLIP_PLANE1_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_CLIP_PLANE1_ENABLE) ? 1 : 0;

        case GL_CLIP_PLANE2_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_CLIP_PLANE2_ENABLE) ? 1 : 0;

        case GL_CLIP_PLANE3_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_CLIP_PLANE3_ENABLE) ? 1 : 0;

        case GL_CLIP_PLANE4_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_CLIP_PLANE4_ENABLE) ? 1 : 0;

        case GL_CLIP_PLANE5_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_CLIP_PLANE5_ENABLE) ? 1 : 0;

        case GL_CULL_FACE_EMU:
            return (ctx->non_shader_statuses_ & egl_context_es1::NON_SHADER_STATE_CULL_FACE_ENABLE) ? 1 : 0;

        case GL_COLOR_MATERIAL_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_COLOR_MATERIAL_ENABLE) ? 1 : 0;

        case GL_DEPTH_TEST_EMU:
            return (ctx->non_shader_statuses_ & egl_context_es1::NON_SHADER_STATE_DEPTH_TEST_ENABLE) ? 1 : 0;

        case GL_STENCIL_TEST_EMU:
            return (ctx->non_shader_statuses_ & egl_context_es1::NON_SHADER_STATE_STENCIL_TEST_ENABLE) ? 1 : 0;

        case GL_FOG_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_FOG_ENABLE) ? 1 : 0;

        case GL_LIGHTING_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHTING_ENABLE) ? 1 : 0;

        case GL_LIGHT0_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT0_ON) ? 1 : 0;

        case GL_LIGHT1_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT1_ON) ? 1 : 0;

        case GL_LIGHT2_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT2_ON) ? 1 : 0;

        case GL_LIGHT3_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT3_ON) ? 1 : 0;

        case GL_LIGHT4_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT4_ON) ? 1 : 0;

        case GL_LIGHT5_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT5_ON) ? 1 : 0;

        case GL_LIGHT6_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT6_ON) ? 1 : 0;
        
        case GL_LIGHT7_EMU:
            return (ctx->fragment_statuses_ & egl_context_es1::FRAGMENT_STATE_LIGHT7_ON) ? 1 : 0;

        case GL_LINE_SMOOTH_EMU:
            return (ctx->non_shader_statuses_ & egl_context_es1::NON_SHADER_STATE_LINE_SMOOTH) ? 1 : 0;

        case GL_DITHER_EMU:
            return (ctx->non_shader_statuses_ & egl_context_es1::NON_SHADER_STATE_DITHER) ? 1 : 0;

        case GL_SCISSOR_TEST_EMU:
            return (ctx->non_shader_statuses_ & egl_context_es1::NON_SHADER_STATE_SCISSOR_ENABLE) ? 1 : 0;

        case GL_SAMPLE_COVERAGE_EMU:
            return (ctx->non_shader_statuses_ & egl_context_es1::NON_SHADER_STATE_SAMPLE_COVERAGE) ? 1 : 0;

        case GL_MULTISAMPLE_EMU:
            return (ctx->non_shader_statuses_ & egl_context_es1::NON_SHADER_STATE_MULTISAMPLE) ? 1 : 0;

        case GL_SAMPLE_ALPHA_TO_COVERAGE_EMU:
            return (ctx->non_shader_statuses_ & egl_context_es1::NON_SHADER_STATE_SAMPLE_ALPHA_TO_COVERAGE) ? 1 : 0;

        case GL_SAMPLE_ALPHA_TO_ONE_EMU:
            return (ctx->non_shader_statuses_ & egl_context_es1::NON_SHADER_STATE_SAMPLE_ALPHA_TO_ONE) ? 1 : 0;

        case GL_TEXTURE_2D_EMU:
            return static_cast<std::int32_t>(ctx->texture_units_[ctx->active_texture_unit_].texturing_enabled_);

        case GL_NORMALIZE_EMU:
            return (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_NORMALIZE) ? 1 : 0;

        case GL_RESCALE_NORMAL_EMU:
            return (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_RESCALE) ? 1 : 0;

        case GL_POLYGON_OFFSET_FILL_EMU:
            return (ctx->non_shader_statuses_ & egl_context_es1::NON_SHADER_STATE_POLYGON_OFFSET_FILL) ? 1 : 0;

        case GL_VERTEX_ARRAY_EMU:
            return (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_VERTEX_ARRAY) ? 1 : 0;

        case GL_COLOR_ARRAY_EMU:
            return (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY) ? 1 : 0;

        case GL_NORMAL_ARRAY_EMU:
            return (ctx->vertex_statuses_ & egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY) ? 1 : 0;

        case GL_TEXTURE_COORD_ARRAY_EMU: {
            std::uint64_t mask = 1 << (egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD_ARRAY_POS +
                static_cast<std::uint8_t>(ctx->active_client_texture_unit_));
            return (ctx->vertex_statuses_ & mask) ? 1 : 0;
        }

        default:
            controller.push_error(ctx, GL_INVALID_ENUM);
            break;
        }

        return 0;
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

        if (!assign_vertex_attrib_gles1(ctx->weight_attrib_, size, type, stride, offset, ctx->binded_array_buffer_handle_)) {
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

        if (!assign_vertex_attrib_gles1(ctx->matrix_index_attrib_, size, type, stride, offset, ctx->binded_array_buffer_handle_)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        // According to the documentation: the number of N is indicated by value passed to glMatrixIndexPointerOES
        // The whole extension document is not too strict.
        ctx->vertex_statuses_ |= ((size & 0b11) << egl_context_es1::VERTEX_STATE_SKIN_WEIGHTS_PER_VERTEX_BITS_POS);
        ctx->attrib_changed_ = true;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_point_size_emu, float size) {
        // Empty
    }

    BRIDGE_FUNC_LIBRARY(void, gl_point_sizex_emu, gl_fixed size) {
        // Empty
    }
}