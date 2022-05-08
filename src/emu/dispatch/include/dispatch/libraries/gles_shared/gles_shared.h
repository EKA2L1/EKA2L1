
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

#include <dispatch/libraries/gles_shared/consts.h>
#include <dispatch/def.h>
#include <cstdint>

namespace eka2l1::dispatch {
    BRIDGE_FUNC_LIBRARY(void, gl_clear_color_emu, float red, float green, float blue, float alpha);
    BRIDGE_FUNC_LIBRARY(void, gl_clear_colorx_emu, gl_fixed red, gl_fixed green, gl_fixed blue, gl_fixed alpha);
    BRIDGE_FUNC_LIBRARY(void, gl_clear_depthf_emu, float depth);
    BRIDGE_FUNC_LIBRARY(void, gl_clear_depthx_emu, gl_fixed depth);
    BRIDGE_FUNC_LIBRARY(void, gl_clear_stencil, std::int32_t s);
    BRIDGE_FUNC_LIBRARY(void, gl_clear_emu, std::uint32_t bits);
    BRIDGE_FUNC_LIBRARY(std::uint32_t, gl_get_error_emu);
    BRIDGE_FUNC_LIBRARY(void, gl_cull_face_emu, std::uint32_t mode);
    BRIDGE_FUNC_LIBRARY(void, gl_scissor_emu, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height);
    BRIDGE_FUNC_LIBRARY(void, gl_front_face_emu, std::uint32_t mode);
    BRIDGE_FUNC_LIBRARY(bool, gl_is_texture_emu, std::uint32_t name);
    BRIDGE_FUNC_LIBRARY(bool, gl_is_buffer_emu, std::uint32_t name);
    BRIDGE_FUNC_LIBRARY(void, gl_gen_textures_emu, std::int32_t n, std::uint32_t *texs);
    BRIDGE_FUNC_LIBRARY(void, gl_gen_buffers_emu, std::int32_t n, std::uint32_t *buffers);
    BRIDGE_FUNC_LIBRARY(void, gl_delete_textures_emu, std::int32_t n, std::uint32_t *texs);
    BRIDGE_FUNC_LIBRARY(void, gl_delete_buffers_emu, std::int32_t n, std::uint32_t *buffers);
    BRIDGE_FUNC_LIBRARY(void, gl_bind_buffer_emu, std::uint32_t target, std::uint32_t name);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t internal_format,
        std::int32_t width, std::int32_t height, std::int32_t border, std::uint32_t format, std::uint32_t data_type,
        void *data_pixels);
    BRIDGE_FUNC_LIBRARY(void, gl_compressed_tex_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t internal_format,
        std::int32_t width, std::int32_t height, std::int32_t border, std::uint32_t image_size, void *data_pixels);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_sub_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t xoffset,
        std::int32_t yoffset, std::int32_t width, std::int32_t height, std::uint32_t format, std::uint32_t data_type,
        void *data_pixels);
    BRIDGE_FUNC_LIBRARY(void, gl_active_texture_emu, std::uint32_t unit);
    BRIDGE_FUNC_LIBRARY(void, gl_bind_texture_emu, std::uint32_t target, std::uint32_t name);
    BRIDGE_FUNC_LIBRARY(void, gl_buffer_data_emu, std::uint32_t target, std::int32_t size, const void *data, std::uint32_t usage);
    BRIDGE_FUNC_LIBRARY(void, gl_buffer_sub_data_emu, std::uint32_t target, std::int32_t offset, std::int32_t size, const void *data);
    BRIDGE_FUNC_LIBRARY(void, gl_color_mask_emu, std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha);
    BRIDGE_FUNC_LIBRARY(void, gl_blend_func_emu, std::uint32_t source_factor, std::uint32_t dest_factor);
    BRIDGE_FUNC_LIBRARY(void, gl_stencil_mask_emu, std::uint32_t mask);
    BRIDGE_FUNC_LIBRARY(void, gl_stencil_mask_separate_emu, std::uint32_t face, std::uint32_t mask);
    BRIDGE_FUNC_LIBRARY(void, gl_depth_mask_emu, std::uint32_t mask);
    BRIDGE_FUNC_LIBRARY(void, gl_depth_func_emu, std::uint32_t func);
    BRIDGE_FUNC_LIBRARY(void, gl_stencil_func_emu, std::uint32_t func, std::int32_t ref, std::uint32_t mask);
    BRIDGE_FUNC_LIBRARY(void, gl_stencil_func_separate_emu, std::uint32_t face, std::uint32_t func, std::int32_t ref, std::uint32_t mask);
    BRIDGE_FUNC_LIBRARY(void, gl_stencil_op_emu, std::uint32_t fail, std::uint32_t depth_fail, std::uint32_t depth_pass);
    BRIDGE_FUNC_LIBRARY(void, gl_stencil_op_separate_emu, std::uint32_t face, std::uint32_t fail, std::uint32_t depth_fail, std::uint32_t depth_pass);
    BRIDGE_FUNC_LIBRARY(void, gl_clip_plane_f_emu, std::uint32_t plane, float *eq);
    BRIDGE_FUNC_LIBRARY(void, gl_clip_plane_x_emu, std::uint32_t plane, gl_fixed *eq);
    BRIDGE_FUNC_LIBRARY(void, gl_viewport_emu, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_i_emu, std::uint32_t target, std::uint32_t pname, std::int32_t param);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_f_emu, std::uint32_t target, std::uint32_t pname, float param);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_x_emu, std::uint32_t target, std::uint32_t pname, gl_fixed param);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_iv_emu, std::uint32_t target, std::uint32_t pname, std::int32_t* param);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_fv_emu, std::uint32_t target, std::uint32_t pname, float* param);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_xv_emu, std::uint32_t target, std::uint32_t pname, gl_fixed* param);
    BRIDGE_FUNC_LIBRARY(void, gl_line_width_emu, float width);
    BRIDGE_FUNC_LIBRARY(void, gl_line_widthx_emu, gl_fixed width);
    BRIDGE_FUNC_LIBRARY(void, gl_finish_emu);
    BRIDGE_FUNC_LIBRARY(void, gl_polygon_offset_emu, float factors, float units);
    BRIDGE_FUNC_LIBRARY(void, gl_polygon_offsetx_emu, gl_fixed factors, gl_fixed units);
    BRIDGE_FUNC_LIBRARY(void, gl_pixel_storei_emu, std::uint32_t pname, std::int32_t param);
    BRIDGE_FUNC_LIBRARY(void, gl_depth_rangef_emu, float near, float far);
    BRIDGE_FUNC_LIBRARY(void, gl_depth_rangex_emu, gl_fixed near, gl_fixed far);
    BRIDGE_FUNC_LIBRARY(void, gl_get_tex_parameter_iv_emu, std::uint32_t target, std::uint32_t pname, std::uint32_t *param);
    BRIDGE_FUNC_LIBRARY(void, gl_get_tex_parameter_fv_emu, std::uint32_t target, std::uint32_t pname, float *param);
    BRIDGE_FUNC_LIBRARY(void, gl_get_tex_parameter_xv_emu, std::uint32_t target, std::uint32_t pname, gl_fixed *param);
    BRIDGE_FUNC_LIBRARY(void, gl_point_size_emu, float size);
    BRIDGE_FUNC_LIBRARY(void, gl_point_sizex_emu, gl_fixed size);
    BRIDGE_FUNC_LIBRARY(void, gl_draw_arrays_emu, std::uint32_t mode, std::int32_t first_index, std::int32_t count);
    BRIDGE_FUNC_LIBRARY(void, gl_draw_elements_emu, std::uint32_t mode, std::int32_t count, const std::uint32_t index_type,
        std::uint32_t indices_ptr);
    BRIDGE_FUNC_LIBRARY(void, gl_blend_func_separate_emu, std::uint32_t source_rgb, std::uint32_t dest_rgb, std::uint32_t source_a, std::uint32_t dest_a);
    BRIDGE_FUNC_LIBRARY(void, gl_blend_equation_emu, std::uint32_t mode);
    BRIDGE_FUNC_LIBRARY(void, gl_blend_equation_separate_emu, std::uint32_t rgb_mode, std::uint32_t a_mode);
    BRIDGE_FUNC_LIBRARY(address, gl_get_string_emu, std::uint32_t pname);
    BRIDGE_FUNC_LIBRARY(void, gl_enable_emu, std::uint32_t cap);
    BRIDGE_FUNC_LIBRARY(void, gl_disable_emu, std::uint32_t cap);
    BRIDGE_FUNC_LIBRARY(void, gl_get_integerv_emu, std::uint32_t pname, std::uint32_t *params);
    BRIDGE_FUNC_LIBRARY(void, gl_get_booleanv_emu, std::uint32_t pname, std::int32_t *params);
    BRIDGE_FUNC_LIBRARY(void, gl_get_fixedv_emu, std::uint32_t pname, gl_fixed *params);
    BRIDGE_FUNC_LIBRARY(void, gl_get_floatv_emu, std::uint32_t pname, float *params);
    BRIDGE_FUNC_LIBRARY(std::int32_t, gl_is_enabled_emu, std::uint32_t cap);
    BRIDGE_FUNC_LIBRARY(void, gl_flush_emu);
}