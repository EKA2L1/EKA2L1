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

#include <mem/ptr.h>

namespace eka2l1::dispatch {
    BRIDGE_FUNC_LIBRARY(std::uint32_t, gl_create_shader_emu, const std::uint32_t shader_type);
    BRIDGE_FUNC_LIBRARY(void, gl_shader_binary_emu);        // Unsupported by emulator
    BRIDGE_FUNC_LIBRARY(void, gl_shader_source_emu, std::uint32_t shader, std::int32_t count, eka2l1::ptr<const char> *strings,
        const std::int32_t *length);
    BRIDGE_FUNC_LIBRARY(void, gl_get_shader_iv_emu, std::uint32_t shader, std::uint32_t pname, std::int32_t *params);
    BRIDGE_FUNC_LIBRARY(void, gl_compile_shader_emu, std::uint32_t shader);
    BRIDGE_FUNC_LIBRARY(bool, gl_is_shader_emu, std::uint32_t name);
    BRIDGE_FUNC_LIBRARY(bool, gl_is_program_emu, std::uint32_t name);
    BRIDGE_FUNC_LIBRARY(void, gl_release_shader_compiler_emu);
    BRIDGE_FUNC_LIBRARY(void, gl_get_shader_source_emu, std::uint32_t shader, std::int32_t buf_size, std::int32_t *actual_length, char *source);
    BRIDGE_FUNC_LIBRARY(void, gl_get_shader_info_log_emu, std::uint32_t shader, std::int32_t max_length, std::int32_t *actual_length, char *info_log);
    BRIDGE_FUNC_LIBRARY(std::uint32_t, gl_create_program_emu);
    BRIDGE_FUNC_LIBRARY(void, gl_attach_shader_emu, std::uint32_t program, std::uint32_t shader);
    BRIDGE_FUNC_LIBRARY(void, gl_detach_shader_emu, std::uint32_t program, std::uint32_t shader);
    BRIDGE_FUNC_LIBRARY(void, gl_link_program_emu, std::uint32_t program);
    BRIDGE_FUNC_LIBRARY(void, gl_validate_program_emu, std::uint32_t program);
    BRIDGE_FUNC_LIBRARY(void, gl_get_program_iv_emu, std::uint32_t program, std::uint32_t pname, std::int32_t *params);
    BRIDGE_FUNC_LIBRARY(void, gl_get_program_info_log_emu, std::uint32_t shader, std::int32_t max_length, std::int32_t *actual_length, char *info_log);
    BRIDGE_FUNC_LIBRARY(void, gl_gen_renderbuffers_emu, std::int32_t count, std::uint32_t *rbs);
    BRIDGE_FUNC_LIBRARY(void, gl_bind_renderbuffer_emu, std::uint32_t target, std::uint32_t rb);
    BRIDGE_FUNC_LIBRARY(bool, gl_is_renderbuffer_emu, std::uint32_t name);
    BRIDGE_FUNC_LIBRARY(void, gl_renderbuffer_storage_emu, std::uint32_t target, std::uint32_t internal_format, std::int32_t width, std::int32_t height);
    BRIDGE_FUNC_LIBRARY(void, gl_gen_framebuffers_emu, std::int32_t count, std::uint32_t *names);
    BRIDGE_FUNC_LIBRARY(bool, gl_is_framebuffer_emu, std::uint32_t name);
    BRIDGE_FUNC_LIBRARY(void, gl_bind_framebuffer_emu, std::uint32_t target, std::uint32_t fb);
    BRIDGE_FUNC_LIBRARY(void, gl_framebuffer_texture2d_emu, std::uint32_t target, std::uint32_t attachment_type,
        std::uint32_t attachment_target, std::uint32_t attachment);
    BRIDGE_FUNC_LIBRARY(void, gl_framebuffer_renderbuffer_emu, std::uint32_t target, std::uint32_t attachment_type,
        std::uint32_t attachment_target, std::uint32_t attachment);
    BRIDGE_FUNC_LIBRARY(std::uint32_t, gl_check_framebuffer_status_emu, std::uint32_t target);
    BRIDGE_FUNC_LIBRARY(void, gl_delete_framebuffers_emu, std::int32_t count, std::uint32_t *names);
    BRIDGE_FUNC_LIBRARY(void, gl_get_active_uniform_emu, std::uint32_t program, std::uint32_t index, std::int32_t buf_size, std::int32_t *buf_written,
        std::int32_t *arr_len, std::uint32_t *var_type, char *name_buf);
    BRIDGE_FUNC_LIBRARY(void, gl_get_active_attrib_emu, std::uint32_t program, std::uint32_t index, std::int32_t buf_size, std::int32_t *buf_written,
        std::int32_t *arr_len, std::uint32_t *var_type, char *name_buf);
    BRIDGE_FUNC_LIBRARY(void, gl_get_attached_shaders_emu, std::uint32_t program, std::int32_t max_buffer_count, std::int32_t *max_buffer_written,
        std::uint32_t *shaders);
    BRIDGE_FUNC_LIBRARY(std::int32_t, gl_get_uniform_location_emu, std::uint32_t program, const char *name);
    BRIDGE_FUNC_LIBRARY(std::int32_t, gl_get_attrib_location_emu, std::uint32_t program, const char *name);
    BRIDGE_FUNC_LIBRARY(void, gl_use_program_emu, std::uint32_t program);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_1f_emu, int location, float v0);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_2f_emu, int location, float v0, float v1);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_3f_emu, int location, float v0, float v1, float v2);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_4f_emu, int location, float v0, float v1, float v2, float v3);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_1fv_emu, int location, std::int32_t count, float *value);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_2fv_emu, int location, std::int32_t count, float *value);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_3fv_emu, int location, std::int32_t count, float *value);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_4fv_emu, int location, std::int32_t count, float *value);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_1i_emu, int location, std::int32_t i0);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_2i_emu, int location, std::int32_t i0, std::int32_t i1);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_3i_emu, int location, std::int32_t i0, std::int32_t i1, std::int32_t i2);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_4i_emu, int location, std::int32_t i0, std::int32_t i1, std::int32_t i2, std::int32_t i3);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_1iv_emu, int location, std::int32_t count, std::int32_t *value);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_2iv_emu, int location, std::int32_t count, std::int32_t *value);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_3iv_emu, int location, std::int32_t count, std::int32_t *value);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_4iv_emu, int location, std::int32_t count, std::int32_t *value);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_matrix_2fv_emu, int location, std::int32_t count, bool transpose, float *value);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_matrix_3fv_emu, int location, std::int32_t count, bool transpose, float *value);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_matrix_4fv_emu, int location, std::int32_t count, bool transpose, float *value);
    BRIDGE_FUNC_LIBRARY(void, gl_uniform_4iv_emu, int location, std::int32_t count, std::int32_t *value);
    BRIDGE_FUNC_LIBRARY(void, gl_enable_vertex_attrib_array_emu, int index);
    BRIDGE_FUNC_LIBRARY(void, gl_disable_vertex_attrib_array_emu, int index);
    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_1f_emu, std::uint32_t index, float v0);
    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_2f_emu, std::uint32_t index, float v0, float v1);
    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_3f_emu, std::uint32_t index, float v0, float v1, float v2);
    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_4f_emu, std::uint32_t index, float v0, float v1, float v2, float v3);
    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_1fv_emu, std::uint32_t index, float *v);
    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_2fv_emu, std::uint32_t index, float *v);
    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_3fv_emu, std::uint32_t index, float *v);
    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_4fv_emu, std::uint32_t index, float *v);
    BRIDGE_FUNC_LIBRARY(void, gl_vertex_attrib_pointer_emu, std::uint32_t index, std::int32_t size, std::uint32_t type, bool normalized,
        std::int32_t stride, address offset);
    BRIDGE_FUNC_LIBRARY(void, gl_bind_attrib_location_emu, std::uint32_t program, std::uint32_t index, const char *name);
    BRIDGE_FUNC_LIBRARY(void, gl_delete_shader_emu, std::uint32_t shader);
}