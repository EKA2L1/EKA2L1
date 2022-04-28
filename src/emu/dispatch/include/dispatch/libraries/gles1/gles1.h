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

#include <dispatch/def.h>
#include <cstdint>

namespace eka2l1::dispatch {
    using gl_fixed = std::int32_t;

    BRIDGE_FUNC_LIBRARY(void, gl_clear_color_emu, float red, float green, float blue, float alpha);
    BRIDGE_FUNC_LIBRARY(void, gl_clear_colorx_emu, gl_fixed red, gl_fixed green, gl_fixed blue, gl_fixed alpha);
    BRIDGE_FUNC_LIBRARY(void, gl_clear_depthf_emu, float depth);
    BRIDGE_FUNC_LIBRARY(void, gl_clear_depthx_emu, gl_fixed depth);
    BRIDGE_FUNC_LIBRARY(void, gl_clear_stencil, std::int32_t s);
    BRIDGE_FUNC_LIBRARY(void, gl_clear_emu, std::uint32_t bits);
    BRIDGE_FUNC_LIBRARY(void, gl_matrix_mode_emu, std::uint32_t mode);
    BRIDGE_FUNC_LIBRARY(void, gl_push_matrix_emu);
    BRIDGE_FUNC_LIBRARY(void, gl_pop_matrix_emu);
    BRIDGE_FUNC_LIBRARY(void, gl_load_identity_emu);
    BRIDGE_FUNC_LIBRARY(void, gl_load_matrixf_emu, float *mat);
    BRIDGE_FUNC_LIBRARY(void, gl_load_matrixx_emu, gl_fixed *mat);
    BRIDGE_FUNC_LIBRARY(std::uint32_t, gl_get_error_emu);
    BRIDGE_FUNC_LIBRARY(void, gl_orthof_emu, float left, float right, float bottom, float top, float near, float far);
    BRIDGE_FUNC_LIBRARY(void, gl_orthox_emu, gl_fixed left, gl_fixed right, gl_fixed bottom, gl_fixed top, gl_fixed near, gl_fixed far);
    BRIDGE_FUNC_LIBRARY(void, gl_mult_matrixf_emu, float *m);
    BRIDGE_FUNC_LIBRARY(void, gl_mult_matrixx_emu, gl_fixed *m);
    BRIDGE_FUNC_LIBRARY(void, gl_scalef_emu, float x, float y, float z);
    BRIDGE_FUNC_LIBRARY(void, gl_scalex_emu, gl_fixed x, gl_fixed y, gl_fixed z);
    BRIDGE_FUNC_LIBRARY(void, gl_translatef_emu, float x, float y, float z);
    BRIDGE_FUNC_LIBRARY(void, gl_translatex_emu, gl_fixed x, gl_fixed y, gl_fixed z);
    BRIDGE_FUNC_LIBRARY(void, gl_rotatef_emu, float angles, float x, float y, float z);
    BRIDGE_FUNC_LIBRARY(void, gl_rotatex_emu, gl_fixed angles, gl_fixed x, gl_fixed y, gl_fixed z);
    BRIDGE_FUNC_LIBRARY(void, gl_frustumf_emu, float left, float right, float bottom, float top, float near, float far);
    BRIDGE_FUNC_LIBRARY(void, gl_frustumx_emu, gl_fixed left, gl_fixed right, gl_fixed bottom, gl_fixed top, gl_fixed near, gl_fixed far);
    BRIDGE_FUNC_LIBRARY(void, gl_cull_face_emu, std::uint32_t mode);
    BRIDGE_FUNC_LIBRARY(void, gl_scissor_emu, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height);
    BRIDGE_FUNC_LIBRARY(void, gl_front_face_emu, std::uint32_t mode);
    BRIDGE_FUNC_LIBRARY(void, gl_front_face_emu, std::uint32_t mode);
    BRIDGE_FUNC_LIBRARY(bool, gl_is_texture_emu, std::uint32_t name);
    BRIDGE_FUNC_LIBRARY(bool, gl_is_buffer_emu, std::uint32_t name);
    BRIDGE_FUNC_LIBRARY(void, gl_gen_textures_emu, std::int32_t n, std::uint32_t *texs);
    BRIDGE_FUNC_LIBRARY(void, gl_gen_buffers_emu, std::int32_t n, std::uint32_t *buffers);
    BRIDGE_FUNC_LIBRARY(void, gl_delete_textures_emu, std::int32_t n, std::uint32_t *texs);
    BRIDGE_FUNC_LIBRARY(void, gl_delete_buffers_emu, std::int32_t n, std::uint32_t *buffers);
    BRIDGE_FUNC_LIBRARY(void, gl_bind_texture_emu, std::uint32_t target, std::uint32_t name);
    BRIDGE_FUNC_LIBRARY(void, gl_bind_buffer_emu, std::uint32_t target, std::uint32_t name);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t internal_format,
        std::int32_t width, std::int32_t height, std::int32_t border, std::uint32_t format, std::uint32_t data_type,
        void *data_pixels);
    BRIDGE_FUNC_LIBRARY(void, gl_compressed_tex_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t internal_format,
        std::int32_t width, std::int32_t height, std::int32_t border, std::uint32_t image_size, void *data_pixels);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_sub_image_2d_emu, std::uint32_t target, std::int32_t level, std::int32_t xoffset,
        std::int32_t yoffset, std::int32_t width, std::int32_t height, std::uint32_t format, std::uint32_t data_type,
        void *data_pixels);
    BRIDGE_FUNC_LIBRARY(void, gl_alpha_func_emu, std::uint32_t func, float ref);
    BRIDGE_FUNC_LIBRARY(void, gl_alpha_func_x_emu, std::uint32_t func, gl_fixed ref);
    BRIDGE_FUNC_LIBRARY(void, gl_normal_3f_emu, float nx, float ny, float nz);
    BRIDGE_FUNC_LIBRARY(void, gl_normal_3x_emu, gl_fixed nx, gl_fixed ny, gl_fixed nz);
    BRIDGE_FUNC_LIBRARY(void, gl_color_4f_emu, float red, float green, float blue, float alpha);
    BRIDGE_FUNC_LIBRARY(void, gl_color_4x_emu, gl_fixed red, gl_fixed green, gl_fixed blue, gl_fixed alpha);
    BRIDGE_FUNC_LIBRARY(void, gl_color_4ub_emu, std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha);
    BRIDGE_FUNC_LIBRARY(void, gl_shade_model_emu, std::uint32_t model);
    BRIDGE_FUNC_LIBRARY(void, gl_active_texture_emu, std::uint32_t unit);
    BRIDGE_FUNC_LIBRARY(void, gl_client_active_texture_emu, std::uint32_t unit);
    BRIDGE_FUNC_LIBRARY(void, gl_multi_tex_coord_4f_emu, std::uint32_t unit, float s, float t, float r, float q);
    BRIDGE_FUNC_LIBRARY(void, gl_multi_tex_coord_4x_emu, std::uint32_t unit, gl_fixed s, gl_fixed t, gl_fixed r, gl_fixed q);
    BRIDGE_FUNC_LIBRARY(void, gl_enable_client_state_emu, std::uint32_t state);
    BRIDGE_FUNC_LIBRARY(void, gl_disable_client_state_emu, std::uint32_t state);
    BRIDGE_FUNC_LIBRARY(void, gl_buffer_data_emu, std::uint32_t target, std::int32_t size, const void *data, std::uint32_t usage);
    BRIDGE_FUNC_LIBRARY(void, gl_buffer_sub_data_emu, std::uint32_t target, std::int32_t offset, std::int32_t size, const void *data);
    BRIDGE_FUNC_LIBRARY(void, gl_color_pointer_emu, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset);
    BRIDGE_FUNC_LIBRARY(void, gl_normal_pointer_emu, std::uint32_t type, std::int32_t stride, std::uint32_t offset);
    BRIDGE_FUNC_LIBRARY(void, gl_vertex_pointer_emu, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset);
    BRIDGE_FUNC_LIBRARY(void, gl_texcoord_pointer_emu, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset);
    BRIDGE_FUNC_LIBRARY(void, gl_material_f_emu, std::uint32_t target, std::uint32_t pname, const float pvalue);
    BRIDGE_FUNC_LIBRARY(void, gl_material_x_emu, std::uint32_t target, std::uint32_t pname, const gl_fixed pvalue);
    BRIDGE_FUNC_LIBRARY(void, gl_material_fv_emu, std::uint32_t target, std::uint32_t pname, const float *pvalue);
    BRIDGE_FUNC_LIBRARY(void, gl_material_xv_emu, std::uint32_t target, std::uint32_t pname, const gl_fixed *pvalue);
    BRIDGE_FUNC_LIBRARY(void, gl_fog_f_emu, std::uint32_t target, const float param);
    BRIDGE_FUNC_LIBRARY(void, gl_fog_x_emu, std::uint32_t target, const gl_fixed param);
    BRIDGE_FUNC_LIBRARY(void, gl_fog_fv_emu, std::uint32_t target, const float *param);
    BRIDGE_FUNC_LIBRARY(void, gl_fog_xv_emu, std::uint32_t target, const gl_fixed *param);
    BRIDGE_FUNC_LIBRARY(void, gl_color_mask_emu, std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha);
    BRIDGE_FUNC_LIBRARY(void, gl_blend_func_emu, std::uint32_t source_factor, std::uint32_t dest_factor);
    BRIDGE_FUNC_LIBRARY(void, gl_stencil_mask_emu, std::uint32_t mask);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_envi_emu, std::uint32_t target, std::uint32_t name, std::int32_t param);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_envf_emu, std::uint32_t target, std::uint32_t name, float param);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_envx_emu, std::uint32_t target, std::uint32_t name, gl_fixed param);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_enviv_emu, std::uint32_t target, std::uint32_t name, const std::int32_t *param);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_envfv_emu, std::uint32_t target, std::uint32_t name, const float *param);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_envxv_emu, std::uint32_t target, std::uint32_t name, const gl_fixed *param);
    BRIDGE_FUNC_LIBRARY(void, gl_get_pointerv_emu, std::uint32_t target, std::uint32_t *off);
    BRIDGE_FUNC_LIBRARY(void, gl_light_f_emu, std::uint32_t light, std::uint32_t pname, float param);
    BRIDGE_FUNC_LIBRARY(void, gl_light_x_emu, std::uint32_t light, std::uint32_t pname, gl_fixed param);
    BRIDGE_FUNC_LIBRARY(void, gl_light_fv_emu, std::uint32_t light, std::uint32_t pname, const float *param);
    BRIDGE_FUNC_LIBRARY(void, gl_light_xv_emu, std::uint32_t light, std::uint32_t pname, const gl_fixed *param);
    BRIDGE_FUNC_LIBRARY(void, gl_depth_mask_emu, std::uint32_t mask);
    BRIDGE_FUNC_LIBRARY(void, gl_light_model_f_emu, std::uint32_t pname, float param);
    BRIDGE_FUNC_LIBRARY(void, gl_light_model_x_emu, std::uint32_t pname, gl_fixed param);
    BRIDGE_FUNC_LIBRARY(void, gl_light_model_fv_emu, std::uint32_t pname, float *param);
    BRIDGE_FUNC_LIBRARY(void, gl_light_model_xv_emu, std::uint32_t pname, gl_fixed *param);
    BRIDGE_FUNC_LIBRARY(void, gl_depth_func_emu, std::uint32_t func);
    BRIDGE_FUNC_LIBRARY(void, gl_stencil_func_emu, std::uint32_t func, std::int32_t ref, std::uint32_t mask);
    BRIDGE_FUNC_LIBRARY(void, gl_stencil_op_emu, std::uint32_t fail, std::uint32_t depth_fail, std::uint32_t depth_pass);
    BRIDGE_FUNC_LIBRARY(void, gl_enable_emu, std::uint32_t cap);
    BRIDGE_FUNC_LIBRARY(void, gl_disable_emu, std::uint32_t cap);
    BRIDGE_FUNC_LIBRARY(void, gl_clip_plane_f_emu, std::uint32_t plane, float *eq);
    BRIDGE_FUNC_LIBRARY(void, gl_clip_plane_x_emu, std::uint32_t plane, gl_fixed *eq);
    BRIDGE_FUNC_LIBRARY(void, gl_viewport_emu, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_i_emu, std::uint32_t target, std::uint32_t pname, std::int32_t param);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_f_emu, std::uint32_t target, std::uint32_t pname, float param);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_x_emu, std::uint32_t target, std::uint32_t pname, gl_fixed param);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_iv_emu, std::uint32_t target, std::uint32_t pname, std::int32_t* param);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_fv_emu, std::uint32_t target, std::uint32_t pname, float* param);
    BRIDGE_FUNC_LIBRARY(void, gl_tex_parameter_xv_emu, std::uint32_t target, std::uint32_t pname, gl_fixed* param);
    BRIDGE_FUNC_LIBRARY(void, gl_draw_arrays_emu, std::uint32_t mode, std::int32_t first_index, std::int32_t count);
    BRIDGE_FUNC_LIBRARY(void, gl_draw_elements_emu, std::uint32_t mode, std::int32_t count, const std::uint32_t index_type,
        std::uint32_t indices_ptr);
    BRIDGE_FUNC_LIBRARY(void, gl_get_integerv_emu, std::uint32_t pname, std::uint32_t *params);
    BRIDGE_FUNC_LIBRARY(void, gl_get_booleanv_emu, std::uint32_t pname, std::int32_t *params);
    BRIDGE_FUNC_LIBRARY(void, gl_get_fixedv_emu, std::uint32_t pname, gl_fixed *params);
    BRIDGE_FUNC_LIBRARY(void, gl_get_floatv_emu, std::uint32_t pname, float *params);
    BRIDGE_FUNC_LIBRARY(address, gl_get_string_emu, std::uint32_t pname);
    BRIDGE_FUNC_LIBRARY(void, gl_hint_emu);
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
    BRIDGE_FUNC_LIBRARY(std::int32_t, gl_is_enabled_emu, std::uint32_t cap);
    BRIDGE_FUNC_LIBRARY(void, gl_load_palette_from_model_view_matrix_oes_emu);
    BRIDGE_FUNC_LIBRARY(void, gl_current_palette_matrix_oes_emu, std::uint32_t index);
    BRIDGE_FUNC_LIBRARY(void, gl_matrix_index_pointer_oes_emu, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset);
    BRIDGE_FUNC_LIBRARY(void, gl_weight_pointer_oes_emu, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset);
    BRIDGE_FUNC_LIBRARY(void, gl_point_size_emu, float size);
    BRIDGE_FUNC_LIBRARY(void, gl_point_sizex_emu, gl_fixed size);
}