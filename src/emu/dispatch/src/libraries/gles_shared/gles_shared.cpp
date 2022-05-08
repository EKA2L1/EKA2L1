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

#include <dispatch/libraries/gles_shared/def.h>
#include <dispatch/libraries/gles_shared/utils.h>
#include <dispatch/libraries/gles_shared/gles_shared.h>
#include <services/window/screen.h>

#include <dispatch/dispatcher.h>
#include <drivers/graphics/graphics.h>
#include <system/epoc.h>
#include <kernel/kernel.h>

namespace eka2l1::dispatch {
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

    bool convert_gl_factor_to_driver_enum(const std::uint32_t value, drivers::blend_factor &dest) {
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

    static bool convert_gl_equation_to_driver_enum(const std::uint32_t value, drivers::blend_equation &eq) {
        switch (value) {
        case GL_FUNC_ADD_EMU:
            eq = drivers::blend_equation::add;
            break;
        
        case GL_FUNC_SUBTRACT_EMU:
            eq = drivers::blend_equation::sub;
            break;

        case GL_FUNC_REVERSE_SUBTRACT_EMU:
            eq = drivers::blend_equation::isub;
            break;

        default:
            return false;
        }

        return true;
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

    bool cond_func_from_gl_enum(const std::uint32_t func, drivers::condition_func &drv_func) {
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

    bool stencil_action_from_gl_enum(const std::uint32_t func, drivers::stencil_action &action) {
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

    bool gl_enum_to_drivers_data_format(const std::uint32_t param, drivers::data_format &res) {
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
    
    bool convert_gl_enum_to_primitive_mode(const std::uint32_t mode, drivers::graphics_primitive_mode &res) {
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
    
    std::uint32_t driver_blend_factor_to_gl_enum(const drivers::blend_factor factor) {
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

    bool is_valid_gl_emu_func(const std::uint32_t func) {
        return ((func == GL_NEVER_EMU) || (func == GL_ALWAYS_EMU) || (func == GL_GREATER_EMU) || (func == GL_GEQUAL_EMU) ||
            (func == GL_LESS_EMU) || (func == GL_LEQUAL_EMU) || (func == GL_EQUAL_EMU) || (func == GL_NOTEQUAL_EMU));
    }

    bool assign_vertex_attrib_gles(gles_vertex_attrib &attrib, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset, std::uint32_t buffer_obj) {
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

    bool egl_context_es_shared::prepare_for_clear(drivers::graphics_driver *driver, egl_controller &controller) {
        flush_state_changes();
        return true;    
    }

    void egl_context_es_shared::flush_to_driver(drivers::graphics_driver *drv, const bool is_frame_swap_flush) {        
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

    void egl_context_es_shared::on_surface_changed(egl_surface *prev_read, egl_surface *prev_draw) {
        // Rebind viewport
        viewport_bl_.top = eka2l1::vec2(0, 0);
        viewport_bl_.size = draw_surface_->dimension_;
    }

    void egl_context_es_shared::flush_state_changes() {
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

        if (state_change_tracker_ & STATE_CHANGED_STENCIL_MASK_FRONT) {
            cmd_builder_.set_stencil_mask(drivers::rendering_face::front, stencil_mask_front_);
        }

        if (state_change_tracker_ & STATE_CHANGED_STENCIL_MASK_BACK) {
            cmd_builder_.set_stencil_mask(drivers::rendering_face::back, stencil_mask_front_);
        }

        if (state_change_tracker_ & STATE_CHANGED_BLEND_FACTOR) {
            cmd_builder_.blend_formula(blend_equation_rgb_, blend_equation_a_, source_blend_factor_rgb_, dest_blend_factor_rgb_,
                source_blend_factor_a_, dest_blend_factor_a_);
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

        if (state_change_tracker_ & STATE_CHANGED_STENCIL_FUNC_FRONT) {
            drivers::condition_func stencil_func_drv;
            cond_func_from_gl_enum(stencil_func_front_, stencil_func_drv);

            cmd_builder_.set_stencil_pass_condition(drivers::rendering_face::front, stencil_func_drv,
                stencil_func_ref_front_, stencil_func_mask_front_);
        }
        
        if (state_change_tracker_ & STATE_CHANGED_STENCIL_FUNC_BACK) {
            drivers::condition_func stencil_func_drv;
            cond_func_from_gl_enum(stencil_func_back_, stencil_func_drv);

            cmd_builder_.set_stencil_pass_condition(drivers::rendering_face::back, stencil_func_drv,
                stencil_func_ref_back_, stencil_func_mask_back_);
        }

        if (state_change_tracker_ & STATE_CHANGED_STENCIL_OP_FRONT) {
            drivers::stencil_action stencil_action_fail_drv, stencil_action_depth_fail_drv, stencil_action_depth_pass_drv;
            stencil_action_from_gl_enum(stencil_fail_action_front_, stencil_action_fail_drv);
            stencil_action_from_gl_enum(stencil_depth_fail_action_front_, stencil_action_depth_fail_drv);
            stencil_action_from_gl_enum(stencil_depth_pass_action_front_, stencil_action_depth_pass_drv);
    
            cmd_builder_.set_stencil_action(drivers::rendering_face::front, stencil_action_fail_drv,
                stencil_action_depth_fail_drv, stencil_action_depth_pass_drv);
        }
        
        if (state_change_tracker_ & STATE_CHANGED_STENCIL_OP_BACK) {
            drivers::stencil_action stencil_action_fail_drv, stencil_action_depth_fail_drv, stencil_action_depth_pass_drv;
            stencil_action_from_gl_enum(stencil_fail_action_back_, stencil_action_fail_drv);
            stencil_action_from_gl_enum(stencil_depth_fail_action_back_, stencil_action_depth_fail_drv);
            stencil_action_from_gl_enum(stencil_depth_pass_action_back_, stencil_action_depth_pass_drv);
    
            cmd_builder_.set_stencil_action(drivers::rendering_face::back, stencil_action_fail_drv,
                stencil_action_depth_fail_drv, stencil_action_depth_pass_drv);
        }

        state_change_tracker_ = 0;
    }

    void egl_context_es_shared::init_context_state() {
        cmd_builder_.bind_bitmap(draw_surface_->handle_, read_surface_->handle_);
        cmd_builder_.set_cull_face(active_cull_face_);
        cmd_builder_.set_front_face_rule(active_front_face_rule_);
        cmd_builder_.set_color_mask(color_mask_);
        cmd_builder_.set_stencil_mask(drivers::rendering_face::front, stencil_mask_front_);
        cmd_builder_.set_stencil_mask(drivers::rendering_face::back, stencil_mask_back_);

        drivers::condition_func depth_func_drv;
        cond_func_from_gl_enum(depth_func_, depth_func_drv);

        drivers::condition_func stencil_func_drv_front;
        drivers::condition_func stencil_func_drv_back;
        cond_func_from_gl_enum(stencil_func_front_, stencil_func_drv_front);
        cond_func_from_gl_enum(stencil_func_back_, stencil_func_drv_back);

        drivers::stencil_action stencil_action_fail_drv_front, stencil_action_depth_fail_drv_front, stencil_action_depth_pass_drv_front,
            stencil_action_fail_drv_back, stencil_action_depth_fail_drv_back, stencil_action_depth_pass_drv_back;
        stencil_action_from_gl_enum(stencil_fail_action_front_, stencil_action_fail_drv_front);
        stencil_action_from_gl_enum(stencil_depth_fail_action_front_, stencil_action_depth_fail_drv_front);
        stencil_action_from_gl_enum(stencil_depth_pass_action_front_, stencil_action_depth_pass_drv_front);
        stencil_action_from_gl_enum(stencil_fail_action_back_, stencil_action_fail_drv_back);
        stencil_action_from_gl_enum(stencil_depth_fail_action_back_, stencil_action_depth_fail_drv_back);
        stencil_action_from_gl_enum(stencil_depth_pass_action_back_, stencil_action_depth_pass_drv_back);

        cmd_builder_.set_depth_pass_condition(depth_func_drv);
        cmd_builder_.set_depth_mask(depth_mask_);
        cmd_builder_.blend_formula(blend_equation_rgb_, blend_equation_a_, source_blend_factor_rgb_, dest_blend_factor_rgb_,
            source_blend_factor_a_, dest_blend_factor_a_);

        cmd_builder_.set_line_width(line_width_);
        cmd_builder_.set_depth_bias(polygon_offset_units_, 1.0, polygon_offset_factor_);
        cmd_builder_.set_depth_range(depth_range_min_, depth_range_max_);
        cmd_builder_.set_stencil_pass_condition(drivers::rendering_face::front, stencil_func_drv_front,
            stencil_func_ref_front_, stencil_func_mask_front_);
        cmd_builder_.set_stencil_pass_condition(drivers::rendering_face::back, stencil_func_drv_back,
            stencil_func_ref_back_, stencil_func_mask_back_);
        cmd_builder_.set_stencil_action(drivers::rendering_face::front, stencil_action_fail_drv_front,
            stencil_action_depth_fail_drv_front, stencil_action_depth_pass_drv_front);
        cmd_builder_.set_stencil_action(drivers::rendering_face::back, stencil_action_fail_drv_back,
            stencil_action_depth_fail_drv_back, stencil_action_depth_pass_drv_back);

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

    void egl_context_es_shared::destroy(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder) {
        for (auto &obj: objects_) {
            if (obj) {
                obj.reset();
            }
        }

        while (!texture_pools_2d_.empty()) {
            drivers::handle h = texture_pools_2d_.top();
            builder.destroy(h);

            texture_pools_2d_.pop();
        }
        
        while (!texture_pools_cube_.empty()) {
            drivers::handle h = texture_pools_cube_.top();
            builder.destroy(h);

            texture_pools_cube_.pop();
        }
        
        while (!buffer_pools_.empty()) {
            drivers::handle h = buffer_pools_.top();
            builder.destroy(h);

            buffer_pools_.pop();
        }
        
        vertex_buffer_pusher_.destroy(builder);
        index_buffer_pusher_.destroy(builder);

        egl_context::destroy(driver, builder);
    }

    gles_buffer_pusher::gles_buffer_pusher() {
        for (std::size_t i = 0; i < MAX_BUFFER_SLOT; i++) {
            buffers_[i] = 0;
            used_size_[i] = 0;
            data_[i] = nullptr;
        }

        current_buffer_ = 0;
        size_per_buffer_ = 0;
    }

    void gles_buffer_pusher::initialize(drivers::graphics_driver *drv, const std::size_t size_per_buffer) {
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

    void gles_buffer_pusher::destroy(drivers::graphics_command_builder &builder) {
        for (std::uint8_t i = 0; i < MAX_BUFFER_SLOT; i++) {
            builder.destroy(buffers_[i]);
            delete data_[i];
        }
    }

    void gles_buffer_pusher::flush(drivers::graphics_command_builder &builder) {
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

    void gles_buffer_pusher::done_frame() {
        current_buffer_ = 0;
    }

    std::uint32_t get_gl_attrib_stride(const gles_vertex_attrib &attrib) {
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
    
    drivers::handle gles_buffer_pusher::push_buffer(const std::uint8_t *data, const gles_vertex_attrib &attrib, const std::int32_t first_index, const std::size_t vert_count, std::size_t &buffer_offset) {
        std::uint32_t stride = get_gl_attrib_stride(attrib);
        std::size_t total_buffer_size = stride * vert_count;

        return push_buffer(data + first_index * stride, total_buffer_size, buffer_offset);
    }

    drivers::handle gles_buffer_pusher::push_buffer(const std::uint8_t *data_source, const std::size_t total_buffer_size, std::size_t &buffer_offset) {
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

    egl_context_es_shared::egl_context_es_shared()
        : egl_context()
        , clear_depth_(1.0f)
        , clear_stencil_(0)
        , active_texture_unit_(0)
        , binded_array_buffer_handle_(0)
        , binded_element_array_buffer_handle_(0)
        , active_cull_face_(drivers::rendering_face::back)
        , active_front_face_rule_(drivers::rendering_face_determine_rule::vertices_counter_clockwise)
        , source_blend_factor_rgb_(drivers::blend_factor::one)
        , dest_blend_factor_rgb_(drivers::blend_factor::zero)
        , source_blend_factor_a_(drivers::blend_factor::one)
        , dest_blend_factor_a_(drivers::blend_factor::zero)
        , blend_equation_rgb_(drivers::blend_equation::add)
        , blend_equation_a_(drivers::blend_equation::add)
        , viewport_bl_(eka2l1::vec2(0, 0), eka2l1::vec2(0, 0))
        , scissor_bl_(eka2l1::vec2(0, 0), eka2l1::vec2(0, 0))
        , line_width_(1.0f)
        , non_shader_statuses_(0)
        , state_change_tracker_(0)
        , color_mask_(0b1111)
        , stencil_mask_front_(0xFFFFFFFF)
        , stencil_mask_back_(0xFFFFFFFF)
        , depth_mask_(0xFFFFFFFF)
        , depth_func_(GL_LESS_EMU)
        , stencil_func_front_(GL_ALWAYS_EMU)
        , stencil_func_mask_front_(0xFFFFFFFF)
        , stencil_func_ref_front_(0)
        , stencil_fail_action_front_(GL_KEEP_EMU)
        , stencil_depth_fail_action_front_(GL_KEEP_EMU)
        , stencil_depth_pass_action_front_(GL_KEEP_EMU)
        , stencil_func_back_(GL_ALWAYS_EMU)
        , stencil_func_mask_back_(0xFFFFFFFF)
        , stencil_func_ref_back_(0)
        , stencil_fail_action_back_(GL_KEEP_EMU)
        , stencil_depth_fail_action_back_(GL_KEEP_EMU)
        , stencil_depth_pass_action_back_(GL_KEEP_EMU)
        , polygon_offset_factor_(0.0f)
        , polygon_offset_units_(0.0f)
        , pack_alignment_(4)
        , unpack_alignment_(4)
        , depth_range_min_(0.0f)
        , depth_range_max_(1.0f)
        , attrib_changed_(false) {
        clear_color_[0] = 0.0f;
        clear_color_[1] = 0.0f;
        clear_color_[2] = 0.0f;
        clear_color_[3] = 0.0f;
    }

    gles_driver_object::gles_driver_object(egl_context_es_shared &ctx)
        : context_(ctx)
        , driver_handle_(0) {
    }

    gles_driver_texture::~gles_driver_texture() {
        if (driver_handle_ != 0) {
            context_.return_handle_to_pool(GLES_OBJECT_TEXTURE, driver_handle_, static_cast<int>(texture_type_));
        }
    }
    
    gles_driver_buffer::~gles_driver_buffer() {
        if (driver_handle_ != 0) {
            context_.return_handle_to_pool(GLES_OBJECT_BUFFER, driver_handle_);
        }
    }

    gles_driver_texture::gles_driver_texture(egl_context_es_shared &ctx)
        : gles_driver_object(ctx)
        , internal_format_(0)
        , min_filter_(GL_NEAREST_MIPMAP_LINEAR_EMU)
        , mag_filter_(GL_LINEAR_EMU)
        , wrap_s_(GL_REPEAT_EMU)
        , wrap_t_(GL_REPEAT_EMU)
        , mip_count_(0)
        , auto_regen_mipmap_(false)
        , mipmap_gened_(false)
        , texture_type_(GLES_DRIVER_TEXTURE_TYPE_NONE)
        , max_anisotrophy_(-1.0f) {
    }
    
    std::uint32_t gles_driver_texture::try_bind(const std::uint32_t type) {
        gles_driver_texture_type ttype = GLES_DRIVER_TEXTURE_TYPE_NONE;
        switch (type) {
        case GL_TEXTURE_2D_EMU:
            ttype = GLES_DRIVER_TEXTURE_TYPE_2D;
            break;

        case GL_TEXTURE_CUBE_MAP_EMU:
            ttype = GLES_DRIVER_TEXTURE_TYPE_CUBE;
            break;

        default:
            return GL_INVALID_ENUM;
        }

        if (texture_type_ == GLES_DRIVER_TEXTURE_TYPE_NONE) {
            texture_type_ = ttype;
            return 0;
        }

        if (texture_type_ != ttype) {
            return GL_INVALID_OPERATION;
        }

        return 0;
    }

    std::uint32_t gles_driver_texture::target_matched(const std::uint32_t type) {
        if (texture_type_ == GLES_DRIVER_TEXTURE_TYPE_NONE) {
            return GL_INVALID_OPERATION;
        }

        gles_driver_texture_type ttype = GLES_DRIVER_TEXTURE_TYPE_NONE;
        switch (type) {
        case GL_TEXTURE_2D_EMU:
            ttype = GLES_DRIVER_TEXTURE_TYPE_2D;
            break;

        case GL_TEXTURE_CUBE_MAP_EMU:
            ttype = GLES_DRIVER_TEXTURE_TYPE_CUBE;
            break;

        default:
            return GL_INVALID_ENUM;
        }

        if (texture_type_ != ttype) {
            return GL_INVALID_OPERATION;
        }

        return 0;
    }

    void gles_driver_texture::try_upscale() {
        float scale = context_.draw_surface_->backed_screen_->display_scale_factor;

        if (!driver_handle_) {
            current_scale_ = scale;
            return;
        }

        if (current_scale_ == scale) {
            return;
        }

        current_scale_ = scale;

        drivers::texture_format internal_format_driver;
        drivers::texture_format format_driver;
        drivers::texture_data_type dtype;
        drivers::channel_swizzles swizzles;

        eka2l1::vec2 size_upscaled = size_ * current_scale_;

        get_data_type_to_upload(internal_format_driver, format_driver, dtype, swizzles, internal_format_, 0);

        if (texture_type_ == GLES_DRIVER_TEXTURE_TYPE_2D) {
            context_.cmd_builder_.recreate_texture(driver_handle_, 2, 0, internal_format_driver, internal_format_driver,
                drivers::texture_data_type::ubyte, nullptr, 0, eka2l1::vec3(size_upscaled.x, size_upscaled.y, 0), 0,
                context_.unpack_alignment_);
        } else {
            for (std::uint32_t i = 0; i < 6; i++) {
                context_.cmd_builder_.recreate_texture(driver_handle_, 4 + i, 0, internal_format_driver, internal_format_driver,
                    drivers::texture_data_type::ubyte, nullptr, 0, eka2l1::vec3(size_upscaled.x, size_upscaled.y, 0), 0,
                    context_.unpack_alignment_);
            }
        }
    }

    gles_driver_buffer::gles_driver_buffer(egl_context_es_shared &ctx)
        : gles_driver_object(ctx)
        , data_size_(0) {
    }

    gles_driver_buffer *egl_context_es_shared::binded_buffer(const bool is_array_buffer) {
        auto *obj = objects_.get(is_array_buffer ? binded_array_buffer_handle_ : binded_element_array_buffer_handle_);
        if (!obj || (*obj)->object_type() != GLES_OBJECT_BUFFER) {
            return nullptr;
        }

        return reinterpret_cast<gles_driver_buffer*>(obj->get());
    }

    void egl_context_es_shared::return_handle_to_pool(const gles_object_type type, const drivers::handle h, const int subtype) {
        switch (type) {
        case GLES_OBJECT_BUFFER:
            buffer_pools_.push(h);
            break;

        case GLES_OBJECT_TEXTURE:
            if (static_cast<gles_driver_texture_type>(subtype) == GLES_DRIVER_TEXTURE_TYPE_2D) {
                texture_pools_2d_.push(h);
            } else {
                texture_pools_cube_.push(h);
            }
            break;

        default:
            break;
        }
    }

    bool egl_context_es_shared::retrieve_vertex_buffer_slot(std::vector<drivers::handle> &vertex_buffers_alloc, drivers::graphics_driver *drv,
        kernel::process *crr_process, const gles_vertex_attrib &attrib, const std::int32_t first_index, const std::int32_t vcount,
        std::uint32_t &res, int &offset, bool &attrib_not_persistent) {
        drivers::handle buffer_handle_drv = 0;
        if (attrib.buffer_obj_ == 0) {
            std::uint8_t *data_raw = eka2l1::ptr<std::uint8_t>(attrib.offset_).get(crr_process);
            if (!data_raw) {
                LOG_ERROR(HLE_DISPATCHER, "Unable to retrieve raw pointer of non-buffer binded attribute!");
                return false;
            }

            if (!vertex_buffer_pusher_.is_initialized()) {
                vertex_buffer_pusher_.initialize(drv, common::MB(4));
            }

            std::size_t offset_big = 0;
            buffer_handle_drv = vertex_buffer_pusher_.push_buffer(data_raw, attrib, first_index, vcount, offset_big);

            offset = static_cast<int>(offset_big);

            if (buffer_handle_drv == 0) {
                // Buffers are full, need flushing all
                flush_to_driver(drv);
                buffer_handle_drv = vertex_buffer_pusher_.push_buffer(data_raw, attrib, first_index, vcount, offset_big);
            }

            if (!attrib_not_persistent) {
                attrib_not_persistent = true;
            }
        } else {
            offset = static_cast<int>(attrib.offset_);
            if (first_index) {
                offset += first_index * get_gl_attrib_stride(attrib);
            }
            auto *buffer_inst_ptr = objects_.get(attrib.buffer_obj_);
            if (!buffer_inst_ptr || ((*buffer_inst_ptr)->object_type() != GLES_OBJECT_BUFFER)) {
                return false;
            }

            gles_driver_buffer *buffer = reinterpret_cast<gles_driver_buffer*>((*buffer_inst_ptr).get());
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
    }

    egl_context_es_shared *get_es_shared_active_context(system *sys) {
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
        if (!context) {
            controller.push_error(crr_thread_uid, GL_INVALID_OPERATION);
            return nullptr;
        }

        return reinterpret_cast<egl_context_es_shared*>(context);
    }

    bool egl_context_es_shared::enable(const std::uint32_t feature) {
        switch (feature) {
        case GL_BLEND_EMU:
            non_shader_statuses_ |= egl_context_es_shared::NON_SHADER_STATE_BLEND_ENABLE;
            cmd_builder_.set_feature(drivers::graphics_feature::blend, true);

            break;

        case GL_COLOR_LOGIC_OP_EMU:
            non_shader_statuses_ |= egl_context_es_shared::NON_SHADER_STATE_COLOR_LOGIC_OP_ENABLE;
            break;

        case GL_CULL_FACE_EMU:
            non_shader_statuses_ |= egl_context_es_shared::NON_SHADER_STATE_CULL_FACE_ENABLE;
            cmd_builder_.set_feature(drivers::graphics_feature::cull, true);

            break;

        case GL_DEPTH_TEST_EMU:
            non_shader_statuses_ |= egl_context_es_shared::NON_SHADER_STATE_DEPTH_TEST_ENABLE;
            cmd_builder_.set_feature(drivers::graphics_feature::depth_test, true);

            break;

        case GL_STENCIL_TEST_EMU:
            non_shader_statuses_ |= egl_context_es_shared::NON_SHADER_STATE_STENCIL_TEST_ENABLE;
            cmd_builder_.set_feature(drivers::graphics_feature::stencil_test, true);

            break;

        case GL_LINE_SMOOTH_EMU:
            non_shader_statuses_ |= egl_context_es_shared::NON_SHADER_STATE_LINE_SMOOTH;
            cmd_builder_.set_feature(drivers::graphics_feature::line_smooth, true);

            break;

        case GL_DITHER_EMU:
            non_shader_statuses_ |= egl_context_es_shared::NON_SHADER_STATE_DITHER;
            cmd_builder_.set_feature(drivers::graphics_feature::dither, true);

            break;

        case GL_SCISSOR_TEST_EMU:
            non_shader_statuses_ |= egl_context_es_shared::NON_SHADER_STATE_SCISSOR_ENABLE;
            cmd_builder_.set_feature(drivers::graphics_feature::clipping, true);

            break;

        case GL_SAMPLE_COVERAGE_EMU:
            non_shader_statuses_ |= egl_context_es_shared::NON_SHADER_STATE_SAMPLE_COVERAGE;
            cmd_builder_.set_feature(drivers::graphics_feature::sample_coverage, true);

            break;

        case GL_MULTISAMPLE_EMU:
            non_shader_statuses_ |= egl_context_es_shared::NON_SHADER_STATE_MULTISAMPLE;
            cmd_builder_.set_feature(drivers::graphics_feature::multisample, true);

            break;

        case GL_SAMPLE_ALPHA_TO_COVERAGE_EMU:
            non_shader_statuses_ |= egl_context_es_shared::NON_SHADER_STATE_SAMPLE_ALPHA_TO_COVERAGE;
            cmd_builder_.set_feature(drivers::graphics_feature::sample_alpha_to_coverage, true);

            break;

        case GL_SAMPLE_ALPHA_TO_ONE_EMU:
            non_shader_statuses_ |= egl_context_es_shared::NON_SHADER_STATE_SAMPLE_ALPHA_TO_ONE;
            cmd_builder_.set_feature(drivers::graphics_feature::sample_alpha_to_one, true);

            break;

        case GL_POLYGON_OFFSET_FILL_EMU:
            non_shader_statuses_ |= egl_context_es_shared::NON_SHADER_STATE_POLYGON_OFFSET_FILL;
            cmd_builder_.set_feature(drivers::graphics_feature::polygon_offset_fill, true);

            break;

        default:
            return false;
        }

        return true;
    }

    bool egl_context_es_shared::disable(const std::uint32_t feature) {
        switch (feature) {
        case GL_BLEND_EMU:
            non_shader_statuses_ &= ~egl_context_es_shared::NON_SHADER_STATE_BLEND_ENABLE;
            cmd_builder_.set_feature(drivers::graphics_feature::blend, false);

            break;

        case GL_COLOR_LOGIC_OP_EMU:
            non_shader_statuses_ &= ~egl_context_es_shared::NON_SHADER_STATE_COLOR_LOGIC_OP_ENABLE;
            break;

        case GL_CULL_FACE_EMU:
            non_shader_statuses_ &= ~egl_context_es_shared::NON_SHADER_STATE_CULL_FACE_ENABLE;
            cmd_builder_.set_feature(drivers::graphics_feature::cull, false);

            break;

        case GL_DEPTH_TEST_EMU:
            non_shader_statuses_ &= ~egl_context_es_shared::NON_SHADER_STATE_DEPTH_TEST_ENABLE;
            cmd_builder_.set_feature(drivers::graphics_feature::depth_test, false);

            break;

        case GL_STENCIL_TEST_EMU:
            non_shader_statuses_ &= ~egl_context_es_shared::NON_SHADER_STATE_STENCIL_TEST_ENABLE;
            cmd_builder_.set_feature(drivers::graphics_feature::stencil_test, false);

            break;

        case GL_LINE_SMOOTH_EMU:
            non_shader_statuses_ &= ~egl_context_es_shared::NON_SHADER_STATE_LINE_SMOOTH;
            cmd_builder_.set_feature(drivers::graphics_feature::line_smooth, false);

            break;

        case GL_DITHER_EMU:
            non_shader_statuses_ &= ~egl_context_es_shared::NON_SHADER_STATE_DITHER;
            cmd_builder_.set_feature(drivers::graphics_feature::dither, false);

            break;

        case GL_SCISSOR_TEST_EMU:
            non_shader_statuses_ &= ~egl_context_es_shared::NON_SHADER_STATE_SCISSOR_ENABLE;
            cmd_builder_.set_feature(drivers::graphics_feature::clipping, false);

            break;

        case GL_SAMPLE_COVERAGE_EMU:
            non_shader_statuses_ &= ~egl_context_es_shared::NON_SHADER_STATE_SAMPLE_COVERAGE;
            cmd_builder_.set_feature(drivers::graphics_feature::sample_coverage, false);

            break;

        case GL_MULTISAMPLE_EMU:
            non_shader_statuses_ &= ~egl_context_es_shared::NON_SHADER_STATE_MULTISAMPLE;
            cmd_builder_.set_feature(drivers::graphics_feature::multisample, false);

            break;

        case GL_SAMPLE_ALPHA_TO_COVERAGE_EMU:
            non_shader_statuses_ &= ~egl_context_es_shared::NON_SHADER_STATE_SAMPLE_ALPHA_TO_COVERAGE;
            cmd_builder_.set_feature(drivers::graphics_feature::sample_alpha_to_coverage, false);

            break;

        case GL_SAMPLE_ALPHA_TO_ONE_EMU:
            non_shader_statuses_ &= ~egl_context_es_shared::NON_SHADER_STATE_SAMPLE_ALPHA_TO_ONE;
            cmd_builder_.set_feature(drivers::graphics_feature::sample_alpha_to_one, false);
            break;

        case GL_POLYGON_OFFSET_FILL_EMU:
            non_shader_statuses_ &= ~egl_context_es_shared::NON_SHADER_STATE_POLYGON_OFFSET_FILL;
            cmd_builder_.set_feature(drivers::graphics_feature::polygon_offset_fill, false);

            break;

        default:
            return false;
        }

        return true;
    }

    bool egl_context_es_shared::is_enabled(const std::uint32_t feature, bool &enabled) {
        switch (feature) {
        case GL_BLEND_EMU:
            enabled = (non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_BLEND_ENABLE) ? 1 : 0;
            break;

        case GL_COLOR_LOGIC_OP_EMU:
            enabled = (non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_COLOR_LOGIC_OP_ENABLE) ? 1 : 0;
            break;

        case GL_CULL_FACE_EMU:
            enabled = (non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_CULL_FACE_ENABLE) ? 1 : 0;
            break;

        case GL_DEPTH_TEST_EMU:
            enabled = (non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_DEPTH_TEST_ENABLE) ? 1 : 0;
            break;

        case GL_STENCIL_TEST_EMU:
            enabled = (non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_STENCIL_TEST_ENABLE) ? 1 : 0;
            break;

        case GL_LINE_SMOOTH_EMU:
            enabled = (non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_LINE_SMOOTH) ? 1 : 0;
            break;

        case GL_DITHER_EMU:
            enabled = (non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_DITHER) ? 1 : 0;
            break;

        case GL_SCISSOR_TEST_EMU:
            enabled = (non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_SCISSOR_ENABLE) ? 1 : 0;
            break;

        case GL_SAMPLE_COVERAGE_EMU:
            enabled = (non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_SAMPLE_COVERAGE) ? 1 : 0;
            break;

        case GL_MULTISAMPLE_EMU:
            enabled = (non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_MULTISAMPLE) ? 1 : 0;
            break;

        case GL_SAMPLE_ALPHA_TO_COVERAGE_EMU:
            enabled = (non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_SAMPLE_ALPHA_TO_COVERAGE) ? 1 : 0;
            break;

        case GL_SAMPLE_ALPHA_TO_ONE_EMU:
            enabled = (non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_SAMPLE_ALPHA_TO_ONE) ? 1 : 0;
            break;

        case GL_POLYGON_OFFSET_FILL_EMU:
            enabled = (non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_POLYGON_OFFSET_FILL) ? 1 : 0;
            break;

        default:
            return false;
        }

        return true;
    }
    
    template <typename T>
    bool get_data_impl(egl_context_es_shared *ctx, drivers::graphics_driver *drv, std::uint32_t pname, T *params, std::uint32_t scale_factor) {
        switch (pname) {
        case GL_ACTIVE_TEXTURE_EMU:
            *params = static_cast<T>(ctx->active_texture_unit_ + GL_TEXTURE0_EMU);
            break;

        case GL_ARRAY_BUFFER_BINDING_EMU:
            *params = static_cast<T>(ctx->binded_array_buffer_handle_);
            break;

        case GL_BLEND_EMU:
            if (ctx->non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_BLEND_ENABLE)
                *params = 1;
            else
                *params = 0;

            break;

        case GL_BLEND_DST_EMU:
            *params = static_cast<T>(driver_blend_factor_to_gl_enum(ctx->dest_blend_factor_rgb_));
            break;

        case GL_BLEND_SRC_EMU:
            *params = static_cast<T>(driver_blend_factor_to_gl_enum(ctx->source_blend_factor_rgb_));
            break;

        case GL_COLOR_CLEAR_VALUE_EMU:
            params[0] = static_cast<T>(ctx->clear_color_[0] * scale_factor);
            params[1] = static_cast<T>(ctx->clear_color_[1] * scale_factor);
            params[2] = static_cast<T>(ctx->clear_color_[2] * scale_factor);
            params[3] = static_cast<T>(ctx->clear_color_[3] * scale_factor);
            break;

        case GL_COLOR_LOGIC_OP_EMU:
            *params = static_cast<T>((ctx->non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_COLOR_LOGIC_OP_ENABLE) ? 1 : 0);
            break;

        case GL_COLOR_WRITEMASK_EMU:
            params[0] = static_cast<T>((ctx->color_mask_ & 1) ? 1 : 0);
            params[1] = static_cast<T>((ctx->color_mask_ & 2) ? 1 : 0);
            params[2] = static_cast<T>((ctx->color_mask_ & 4) ? 1 : 0);
            params[3] = static_cast<T>((ctx->color_mask_ & 8) ? 1 : 0);

            break;

        case GL_CULL_FACE_EMU:
            *params = static_cast<T>((ctx->non_shader_statuses_ & egl_context_es_shared::NON_SHADER_STATE_CULL_FACE_ENABLE) ? 1 : 0);
            break;

        case GL_CULL_FACE_MODE_EMU:
            *params = static_cast<T>((ctx->active_cull_face_ == drivers::rendering_face::front) ? GL_FRONT_EMU : GL_BACK_EMU);
            break;

        case GL_DEPTH_CLEAR_EMU:
            *params = static_cast<T>(ctx->clear_depth_ * scale_factor);
            break;

        case GL_MAX_VIEWPORT_DIMS_EMU:
        case GL_MAX_TEXTURE_SIZE_EMU:
            // We clamp on this
            *params = static_cast<T>(GLES_EMU_MAX_TEXTURE_SIZE);
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
                return false;
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

        default:
            return false;
        }

        return true;
    }

    bool egl_context_es_shared::get_data(drivers::graphics_driver *drv, const std::uint32_t feature, void *data, gles_get_data_type data_type) {
        switch (data_type) {
        case GLES_GET_DATA_TYPE_BOOLEAN:
            return get_data_impl<std::int32_t>(this, drv, feature, reinterpret_cast<std::int32_t*>(data), 255);

        case GLES_GET_DATA_TYPE_FIXED:
            return get_data_impl<gl_fixed>(this, drv, feature, reinterpret_cast<gl_fixed*>(data), 65536);

        case GLES_GET_DATA_TYPE_FLOAT:
            return get_data_impl<float>(this, drv, feature, reinterpret_cast<float*>(data), 1);

        case GLES_GET_DATA_TYPE_INTEGER:
            return get_data_impl<std::uint32_t>(this, drv, feature, reinterpret_cast<std::uint32_t*>(data), 255);

        default:
            break;
        }

        return false;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_clear_color_emu, float red, float green, float blue, float alpha) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
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
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->clear_depth_ = depth;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_clear_depthx_emu, gl_fixed depth) {
        gl_clear_depthf_emu(sys, FIXED_32_TO_FLOAT(depth));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_clear_stencil, std::int32_t s) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->clear_stencil_ = s;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_clear_emu, std::uint32_t bits) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
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

        if (!ctx->prepare_for_clear(sys->get_graphics_driver(), sys->get_dispatcher()->get_egl_controller())) {
            return;
        }

        ctx->cmd_builder_.clear(clear_parameters, flags_driver);

        if (ctx->cmd_builder_.need_flush()) {
            ctx->flush_to_driver(sys->get_graphics_driver());
        }
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
    

    BRIDGE_FUNC_LIBRARY(void, gl_cull_face_emu, std::uint32_t mode) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
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

        ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_CULL_FACE;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_scissor_emu, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        // Emulator graphics abstraction use top-left as origin.
        ctx->scissor_bl_.top = eka2l1::vec2(x, y);
        ctx->scissor_bl_.size = eka2l1::vec2(width, height);

        ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_SCISSOR_RECT;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_front_face_emu, std::uint32_t mode) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
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

        ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_FRONT_FACE_RULE;
    }
    
    BRIDGE_FUNC_LIBRARY(bool, gl_is_texture_emu, std::uint32_t name) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return false;
        }

        gles_driver_object_instance *obj = ctx->objects_.get(name);
        return obj && ((*obj)->object_type() == GLES_OBJECT_TEXTURE);
    }

    BRIDGE_FUNC_LIBRARY(bool, gl_is_buffer_emu, std::uint32_t name) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return false;
        }

        gles_driver_object_instance *obj = ctx->objects_.get(name);
        return obj && ((*obj)->object_type() == GLES_OBJECT_BUFFER);
    }
    
    static void gen_gles_objects_generic(system *sys, gles_object_type obj_type, std::int32_t n, std::uint32_t *texs) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
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

        gles_driver_object_instance stub_obj;

        for (std::int32_t i = 0; i < n; i++) {
            switch (obj_type) {
            case GLES_OBJECT_TEXTURE:
                stub_obj = std::make_unique<gles_driver_texture>(*ctx);
                break;

            case GLES_OBJECT_BUFFER:
                stub_obj = std::make_unique<gles_driver_buffer>(*ctx);
                break;

            default:
                break;
            }
            texs[i] = static_cast<std::uint32_t>(ctx->objects_.add(stub_obj));
        }
    }

    void delete_gles_objects_generic(system *sys, gles_object_type obj_type, std::int32_t n, std::uint32_t *names) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
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
        gen_gles_objects_generic(sys, GLES_OBJECT_TEXTURE, n, texs);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_gen_buffers_emu, std::int32_t n, std::uint32_t *buffers) {
        gen_gles_objects_generic(sys, GLES_OBJECT_BUFFER, n, buffers);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_delete_textures_emu, std::int32_t n, std::uint32_t *texs) {
        delete_gles_objects_generic(sys, GLES_OBJECT_TEXTURE, n, texs);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_delete_buffers_emu, std::int32_t n, std::uint32_t *buffers) {
        delete_gles_objects_generic(sys, GLES_OBJECT_BUFFER, n, buffers);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_bind_buffer_emu, std::uint32_t target, std::uint32_t name) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
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
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((width < 0) || (height < 0) || (width > GLES_EMU_MAX_TEXTURE_SIZE) || (height > GLES_EMU_MAX_TEXTURE_SIZE) || !data_pixels) {
            controller.push_error(ctx, GL_INVALID_VALUE);
        }

        gles_driver_texture *tex = ctx->binded_texture();
        if (!tex) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }
        
        std::uint32_t target_match_err = tex->target_matched(target);
        if (target_match_err != 0) {
            controller.push_error(ctx, target_match_err);
            return;
        }

        if ((target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X_EMU) && (target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_EMU)) {
            if (width != height) {
                controller.push_error(ctx, GL_INVALID_VALUE);
                return;
            }
        }

        bool need_reinstantiate = true;

        drivers::texture_format internal_format_driver;
        std::uint8_t dimension = (target == GL_TEXTURE_2D_EMU) ? 2 : static_cast<std::uint8_t>(target - GL_TEXTURE_CUBE_MAP_POSITIVE_X_EMU + 4);

        if ((internal_format >= GL_PALETTE4_RGBA8_OES_EMU) && (internal_format <= GL_PALETTE8_RGB5_A1_OES_EMU)) {
            if (level > 0) {
                controller.push_error(ctx, GL_INVALID_ENUM);
            } else {
                level = (-level) + 1;

                if (level > GLES_EMU_MAX_TEXTURE_MIP_LEVEL) {
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
                if ((dimension > 3) && (!ctx->texture_pools_cube_.empty())) {
                    tex->assign_handle(ctx->texture_pools_cube_.top());
                    ctx->texture_pools_cube_.pop();
                } else if ((dimension == 2) && (!ctx->texture_pools_2d_.empty())) {
                    tex->assign_handle(ctx->texture_pools_2d_.top());
                    ctx->texture_pools_2d_.pop();
                } else {
                    drivers::graphics_driver *drv = sys->get_graphics_driver();
                    drivers::handle new_h = drivers::create_texture(drv, dimension, 0, internal_format_driver,
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
                ctx->cmd_builder_.recreate_texture(tex->handle_value(), dimension, 0, internal_format_driver,
                    internal_format_driver, dtype, data_to_pass, out_size[0], eka2l1::vec3(width, height, 0),
                    0, ctx->unpack_alignment_);
            }

            data_to_pass += out_size[0];

            for (std::size_t i = 1; i < out_size.size(); i++) {
                width /= 2;
                height /= 2;

                ctx->cmd_builder_.recreate_texture(tex->handle_value(), dimension, static_cast<std::uint8_t>(i), internal_format_driver,
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
                if ((dimension > 3) && (!ctx->texture_pools_cube_.empty())) {
                    tex->assign_handle(ctx->texture_pools_cube_.top());
                    ctx->texture_pools_cube_.pop();
                } else if ((dimension == 2) && (!ctx->texture_pools_2d_.empty())) {
                    tex->assign_handle(ctx->texture_pools_2d_.top());
                    ctx->texture_pools_2d_.pop();
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
        tex->set_mipmap_generated(false);

        if (tex->auto_regenerate_mipmap()) {
            ctx->cmd_builder_.regenerate_mips(tex->handle_value());
            tex->set_mipmap_generated(true);
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_tex_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t internal_format,
        std::int32_t width, std::int32_t height, std::int32_t border, std::uint32_t format, std::uint32_t data_type,
        void *data_pixels) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((level < 0) || (level > GLES_EMU_MAX_TEXTURE_MIP_LEVEL) || (width < 0) || (height < 0) || (width > GLES_EMU_MAX_TEXTURE_SIZE)
            || (height > GLES_EMU_MAX_TEXTURE_SIZE)) {
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

        gles_driver_texture *tex = ctx->binded_texture();
        if (!tex) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        std::uint32_t target_match_err = tex->target_matched(target);
        if (target_match_err != 0) {
            controller.push_error(ctx, target_match_err);
            return;
        }

        if ((target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X_EMU) && (target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_EMU)) {
            if (width != height) {
                controller.push_error(ctx, GL_INVALID_VALUE);
                return;
            }
        }

        bool need_reinstantiate = true;
        std::uint8_t dimension = (target == GL_TEXTURE_2D_EMU) ? 2 : static_cast<std::uint8_t>(target - GL_TEXTURE_CUBE_MAP_POSITIVE_X_EMU + 4);

        drivers::texture_format internal_format_driver;
        drivers::texture_format format_driver;
        drivers::texture_data_type dtype;
        drivers::channel_swizzles swizzles;

        get_data_type_to_upload(internal_format_driver, format_driver, dtype, swizzles, format, data_type);

        // TODO: border is ignored!
        if (!tex->handle_value()) {
            if ((dimension > 3) && (!ctx->texture_pools_cube_.empty())) {
                tex->assign_handle(ctx->texture_pools_cube_.top());
                ctx->texture_pools_cube_.pop();
            } else if ((dimension == 2) && (!ctx->texture_pools_2d_.empty())) {
                tex->assign_handle(ctx->texture_pools_2d_.top());
                ctx->texture_pools_2d_.pop();
            } else {
                drivers::graphics_driver *drv = sys->get_graphics_driver();
                drivers::handle new_h = drivers::create_texture(drv, dimension, static_cast<std::uint8_t>(level), internal_format_driver,
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
            ctx->cmd_builder_.recreate_texture(tex->handle_value(), dimension, static_cast<std::uint8_t>(level), internal_format_driver,
                format_driver, dtype, data_pixels, needed_size, eka2l1::vec3(width, height, 0), 0, ctx->unpack_alignment_);
        }

        tex->set_internal_format(format);
        tex->set_size(eka2l1::vec2(width, height));
        tex->set_mip_count(common::max(tex->get_mip_count(), static_cast<std::uint32_t>(level)));
        tex->set_mipmap_generated(false);

        if (tex->auto_regenerate_mipmap()) {
            ctx->cmd_builder_.regenerate_mips(tex->handle_value());
            tex->set_mipmap_generated(true);
        }

        ctx->cmd_builder_.set_texture_max_mip(tex->handle_value(), tex->get_mip_count());
        ctx->cmd_builder_.set_swizzle(tex->handle_value(), swizzles[0], swizzles[1], swizzles[2], swizzles[3]);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_tex_sub_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t xoffset,
        std::int32_t yoffset, std::int32_t width, std::int32_t height, std::uint32_t format, std::uint32_t data_type,
        void *data_pixels) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((level < 0) || (level > GLES_EMU_MAX_TEXTURE_MIP_LEVEL) || (width < 0) || (height < 0) || (width > GLES_EMU_MAX_TEXTURE_SIZE)
            || (height > GLES_EMU_MAX_TEXTURE_SIZE) || !data_pixels) {
            controller.push_error(ctx, GL_INVALID_VALUE);
        }

        const std::uint32_t error = is_format_and_data_type_ok(format, data_type);
        if (error != 0) {
            controller.push_error(ctx, error);
            return;
        }

        gles_driver_texture *tex = ctx->binded_texture();
        if (!tex || !tex->handle_value()) {
            controller.push_error(ctx, GL_INVALID_OPERATION);
            return;
        }

        std::uint32_t target_match_err = tex->target_matched(target);
        if (target_match_err != 0) {
            controller.push_error(ctx, target_match_err);
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

        tex->set_mipmap_generated(false);

        if (tex->auto_regenerate_mipmap()) {
            ctx->cmd_builder_.regenerate_mips(tex->handle_value());
            tex->set_mipmap_generated(true);
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_active_texture_emu, std::uint32_t unit) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
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

    BRIDGE_FUNC_LIBRARY(void, gl_bind_texture_emu, std::uint32_t target, std::uint32_t name) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        std::uint32_t res = ctx->bind_texture(target, name);
        if (res != 0) {
            controller.push_error(ctx, res);
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_buffer_data_emu, std::uint32_t target, std::int32_t size, const void *data, std::uint32_t usage) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        gles_driver_buffer *buffer = ctx->binded_buffer(target == GL_ARRAY_BUFFER_EMU);
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
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        gles_driver_buffer *buffer = ctx->binded_buffer(target == GL_ARRAY_BUFFER_EMU);
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

    BRIDGE_FUNC_LIBRARY(void, gl_color_mask_emu, std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->color_mask_ = 0;
        if (red) ctx->color_mask_ |= 1;
        if (green) ctx->color_mask_ |= 2;
        if (blue) ctx->color_mask_ |= 4;
        if (alpha) ctx->color_mask_ |= 8;

        ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_COLOR_MASK;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_blend_func_emu, std::uint32_t source_factor, std::uint32_t dest_factor) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
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

        ctx->source_blend_factor_rgb_ = ctx->source_blend_factor_a_ = source_factor_driver;
        ctx->dest_blend_factor_rgb_ = ctx->dest_blend_factor_a_ = dest_factor_driver;

        ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_BLEND_FACTOR;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_stencil_mask_emu, std::uint32_t mask) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        if (ctx->stencil_mask_front_ != mask) {
            ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_STENCIL_MASK_FRONT;
        }

        if (ctx->stencil_mask_back_ != mask) {
            ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_STENCIL_MASK_BACK;
        }

        ctx->stencil_mask_front_ = ctx->stencil_mask_back_ = mask;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_stencil_mask_separate_emu, std::uint32_t face, std::uint32_t mask) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }
        
        drivers::condition_func func_drv = drivers::condition_func::always;
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((face != GL_FRONT_EMU) && (face != GL_BACK_EMU) && (face != GL_FRONT_AND_BACK_EMU)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if ((face == GL_FRONT_EMU) || (face == GL_FRONT_AND_BACK_EMU)) {
            if (ctx->stencil_mask_front_ != mask) {
                ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_STENCIL_MASK_FRONT;
            }

            ctx->stencil_mask_front_ = mask;
        }

        if ((face == GL_BACK_EMU) || (face == GL_FRONT_AND_BACK_EMU)) {
            if (ctx->stencil_mask_back_ != mask) {
                ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_STENCIL_MASK_BACK;
            }

            ctx->stencil_mask_back_ = mask;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_depth_mask_emu, std::uint32_t mask) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        if (ctx->depth_mask_ != mask) {
            ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_DEPTH_MASK;
        }

        ctx->depth_mask_ = mask;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_depth_func_emu, std::uint32_t func) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
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
            ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_DEPTH_PASS_COND;
        }

        ctx->depth_func_ = func;
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_stencil_func_emu, std::uint32_t func, std::int32_t ref, std::uint32_t mask) {
        gl_stencil_func_separate_emu(sys, GL_FRONT_AND_BACK_EMU, func, ref, mask);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_stencil_func_separate_emu, std::uint32_t face, std::uint32_t func, std::int32_t ref, std::uint32_t mask) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        drivers::condition_func func_drv = drivers::condition_func::always;
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((face != GL_FRONT_EMU) && (face != GL_BACK_EMU) && (face != GL_FRONT_AND_BACK_EMU)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if (!cond_func_from_gl_enum(func, func_drv)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if ((face == GL_FRONT_EMU) || (face == GL_FRONT_AND_BACK_EMU)) {
            if ((ctx->stencil_func_front_ != func) || (ctx->stencil_func_ref_front_ != ref) || (ctx->stencil_func_mask_front_ != mask)) {
                ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_STENCIL_FUNC_FRONT;
            }

            ctx->stencil_func_front_ = func;
            ctx->stencil_func_ref_front_ = ref;
            ctx->stencil_func_mask_front_ = mask;
        }
        
        if ((face == GL_BACK_EMU) || (face == GL_FRONT_AND_BACK_EMU)) {
            if ((ctx->stencil_func_back_ != func) || (ctx->stencil_func_ref_back_ != ref) || (ctx->stencil_func_mask_back_ != mask)) {
                ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_STENCIL_FUNC_BACK;
            }

            ctx->stencil_func_back_ = func;
            ctx->stencil_func_ref_back_ = ref;
            ctx->stencil_func_mask_back_ = mask;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_stencil_op_emu, std::uint32_t fail, std::uint32_t depth_fail, std::uint32_t depth_pass) {
        gl_stencil_op_separate_emu(sys, GL_FRONT_AND_BACK_EMU, fail, depth_fail, depth_pass);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_stencil_op_separate_emu, std::uint32_t face, std::uint32_t fail, std::uint32_t depth_fail, std::uint32_t depth_pass) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((face != GL_FRONT_EMU) && (face != GL_BACK_EMU) && (face != GL_FRONT_AND_BACK_EMU)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        drivers::stencil_action action_temp = drivers::stencil_action::keep;
        if (!stencil_action_from_gl_enum(fail, action_temp) || !stencil_action_from_gl_enum(depth_fail, action_temp) ||
            !stencil_action_from_gl_enum(depth_pass, action_temp)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        if ((face == GL_FRONT_EMU) || (face == GL_FRONT_AND_BACK_EMU)) {
            if ((ctx->stencil_fail_action_front_ != fail) || (ctx->stencil_depth_fail_action_front_ != depth_fail) ||
                (ctx->stencil_depth_pass_action_front_ != depth_pass)) {
                ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_STENCIL_OP_FRONT;
            }

            ctx->stencil_fail_action_front_ = fail;
            ctx->stencil_depth_fail_action_front_ = depth_fail;
            ctx->stencil_depth_pass_action_front_ = depth_pass;
        }
        
        if ((face == GL_BACK_EMU) || (face == GL_FRONT_AND_BACK_EMU)) {
            if ((ctx->stencil_fail_action_back_ != fail) || (ctx->stencil_depth_fail_action_back_ != depth_fail) ||
                (ctx->stencil_depth_pass_action_back_ != depth_pass)) {
                ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_STENCIL_OP_BACK;
            }

            ctx->stencil_fail_action_back_ = fail;
            ctx->stencil_depth_fail_action_back_ = depth_fail;
            ctx->stencil_depth_pass_action_back_ = depth_pass;
        }
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_viewport_emu, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->viewport_bl_.top = eka2l1::vec2(x, y);
        ctx->viewport_bl_.size = eka2l1::vec2(width, height);

        ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_VIEWPORT_RECT;
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
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        gles_driver_texture *tex = ctx->binded_texture();
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
            egl_context_es_shared *ctx = get_es_shared_active_context(sys);
            if (!ctx) {
                return;
            }

            gles_driver_texture *tex = ctx->binded_texture();
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

    BRIDGE_FUNC_LIBRARY(void, gl_line_width_emu, float width) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
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
        ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_LINE_WIDTH;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_line_widthx_emu, gl_fixed width) {
        gl_line_width_emu(sys, FIXED_32_TO_FLOAT(width));
    }

    
    BRIDGE_FUNC_LIBRARY(void, gl_finish_emu) {
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_flush_emu) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->flush_to_driver(sys->get_graphics_driver());
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_polygon_offset_emu, float factors, float units) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        ctx->polygon_offset_factor_ = factors;
        ctx->polygon_offset_units_ = units;

        ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_DEPTH_BIAS;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_polygon_offsetx_emu, gl_fixed factors, gl_fixed units) {
        gl_polygon_offset_emu(sys, FIXED_32_TO_FLOAT(factors), FIXED_32_TO_FLOAT(units));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_pixel_storei_emu, std::uint32_t pname, std::int32_t param) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
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
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        if ((ctx->depth_range_max_ != far) || (ctx->depth_range_min_ != near)) {
            ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_DEPTH_RANGE;
        }

        ctx->depth_range_min_ = near;
        ctx->depth_range_max_ = far;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_depth_rangex_emu, gl_fixed near, gl_fixed far) {
        gl_depth_rangef_emu(sys, FIXED_32_TO_FLOAT(near), FIXED_32_TO_FLOAT(far));
    }

    BRIDGE_FUNC_LIBRARY(void, gl_get_tex_parameter_iv_emu, std::uint32_t target, std::uint32_t pname, std::uint32_t *param) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (target != GL_TEXTURE_2D_EMU) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        gles_driver_texture *tex = ctx->binded_texture();
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
    
    BRIDGE_FUNC_LIBRARY(void, gl_point_size_emu, float size) {
        // Empty
    }

    BRIDGE_FUNC_LIBRARY(void, gl_point_sizex_emu, gl_fixed size) {
        // Empty
    }
    

    BRIDGE_FUNC_LIBRARY(void, gl_draw_arrays_emu, std::uint32_t mode, std::int32_t first_index, std::int32_t count) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
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

        if (!ctx->prepare_for_draw(drv, controller, sys->get_kernel_system()->crr_process(), first_index, count)) {
            LOG_ERROR(HLE_DISPATCHER, "Error while preparing GLES draw. This should not happen!");
            return;
        }

        ctx->cmd_builder_.draw_arrays(prim_mode_drv, 0, count, false);
 
        if (ctx->cmd_builder_.need_flush()) {
            ctx->flush_to_driver(drv);
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_draw_elements_emu, std::uint32_t mode, std::int32_t count, const std::uint32_t index_type,
        std::uint32_t indices_ptr) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
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

        gles_driver_buffer *binded_elem_buffer_managed = ctx->binded_buffer(false);

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

        if (!ctx->prepare_for_draw(drv, controller, sys->get_kernel_system()->crr_process(), 0, total_vert)) {
            LOG_ERROR(HLE_DISPATCHER, "Error while preparing GLES draw. This should not happen!");
            return;
        }

        ctx->cmd_builder_.draw_indexed(prim_mode_drv, count, index_format_drv, static_cast<int>(indices_ptr), 0);

        if (ctx->cmd_builder_.need_flush()) {
            ctx->flush_to_driver(drv);
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_blend_func_separate_emu, std::uint32_t source_rgb, std::uint32_t dest_rgb, std::uint32_t source_a, std::uint32_t dest_a) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        drivers::blend_factor source_factor_driver;
        drivers::blend_factor dest_factor_driver;

        if (!convert_gl_factor_to_driver_enum(source_rgb, source_factor_driver) || !convert_gl_factor_to_driver_enum(dest_rgb, dest_factor_driver)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->source_blend_factor_rgb_ = source_factor_driver;
        ctx->dest_blend_factor_rgb_ = dest_factor_driver;
        
        if (!convert_gl_factor_to_driver_enum(source_a, source_factor_driver) || !convert_gl_factor_to_driver_enum(dest_a, dest_factor_driver)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
        
        ctx->source_blend_factor_a_ = source_factor_driver;
        ctx->dest_blend_factor_a_ = dest_factor_driver;

        ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_BLEND_FACTOR;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_blend_equation_emu, std::uint32_t mode) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        drivers::blend_equation driver_equation = drivers::blend_equation::add;

        if (!convert_gl_equation_to_driver_enum(mode, driver_equation)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->blend_equation_rgb_ = ctx->blend_equation_a_ = driver_equation;
        ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_BLEND_FACTOR;
    }

    BRIDGE_FUNC_LIBRARY(void, gl_blend_equation_separate_emu, std::uint32_t rgb_mode, std::uint32_t a_mode) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();
        drivers::blend_equation driver_equation_rgb = drivers::blend_equation::add;
        drivers::blend_equation driver_equation_a = drivers::blend_equation::add;

        if (!convert_gl_equation_to_driver_enum(rgb_mode, driver_equation_rgb) ||
            !convert_gl_equation_to_driver_enum(a_mode, driver_equation_a)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }

        ctx->blend_equation_rgb_ = driver_equation_rgb;
        ctx->blend_equation_a_ = driver_equation_a;

        ctx->state_change_tracker_ |= egl_context_es_shared::STATE_CHANGED_BLEND_FACTOR;
    }

    BRIDGE_FUNC_LIBRARY(address, gl_get_string_emu, std::uint32_t pname) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return 0;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if ((pname != GL_EXTENSIONS) && (pname != GL_VENDOR) && (pname != GL_RENDERER) && (pname != GL_VERSION) && (pname != GL_SHADING_LANGUAGE_VERSION_EMU)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return 0;
        }

        // Move to ES2 enumerators!
        if ((ctx->context_type() == EGL_GLES2_CONTEXT) && (pname != GL_SHADING_LANGUAGE_VERSION_EMU)) {
            pname = (pname - GLES_STATIC_STRING_KEY_VENDOR) + GLES_STATIC_STRING_KEY_VENDOR_ES2;
        }

        address res = dp->retrieve_static_string(pname);
        if (res == 0) {
            controller.push_error(ctx, GL_INVALID_ENUM);
        }
        return res;
    }
    
    // NOTE: boolean cast not handled!
    void gl_get_implv_emu(eka2l1::system *sys, std::uint32_t pname, void *params, gles_get_data_type data_type) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
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

        if (!ctx->get_data(drv, pname, params, data_type)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return;
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_get_floatv_emu, std::uint32_t pname, float *params) {
        gl_get_implv_emu(sys, pname, params, GLES_GET_DATA_TYPE_FLOAT);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_get_integerv_emu, std::uint32_t pname, std::uint32_t *params) {
        gl_get_implv_emu(sys, pname, params, GLES_GET_DATA_TYPE_INTEGER);
    }
    
    BRIDGE_FUNC_LIBRARY(void, gl_get_fixedv_emu, std::uint32_t pname, gl_fixed *params) {
        gl_get_implv_emu(sys, pname, params, GLES_GET_DATA_TYPE_FIXED);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_get_booleanv_emu, std::uint32_t pname, std::int32_t *params) {
        gl_get_implv_emu(sys, pname, params, GLES_GET_DATA_TYPE_BOOLEAN);
    }

    BRIDGE_FUNC_LIBRARY(void, gl_enable_emu, std::uint32_t cap) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!ctx->enable(cap)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
        }
    }

    BRIDGE_FUNC_LIBRARY(void, gl_disable_emu, std::uint32_t cap) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return;
        }

        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        if (!ctx->disable(cap)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
        }
    }

    BRIDGE_FUNC_LIBRARY(std::int32_t, gl_is_enabled_emu, std::uint32_t cap) {
        egl_context_es_shared *ctx = get_es_shared_active_context(sys);
        if (!ctx) {
            return 0;
        }
        
        dispatcher *dp = sys->get_dispatcher();
        dispatch::egl_controller &controller = dp->get_egl_controller();

        bool is_enabled_result = false;
        if (!ctx->is_enabled(cap, is_enabled_result)) {
            controller.push_error(ctx, GL_INVALID_ENUM);
            return 0;
        }

        return static_cast<std::int32_t>(is_enabled_result);
    }
}