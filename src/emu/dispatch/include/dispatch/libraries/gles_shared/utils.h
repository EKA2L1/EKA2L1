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

#include <drivers/graphics/common.h>
#include <cstdint>

namespace eka2l1::dispatch {
    struct gles_vertex_attrib;

    bool convert_gl_factor_to_driver_enum(const std::uint32_t value, drivers::blend_factor &dest);
    bool cond_func_from_gl_enum(const std::uint32_t func, drivers::condition_func &drv_func);
    bool stencil_action_from_gl_enum(const std::uint32_t func, drivers::stencil_action &action);
    std::uint32_t get_gl_attrib_stride(const gles_vertex_attrib &attrib);
    bool gl_enum_to_drivers_data_format(const std::uint32_t param, drivers::data_format &res);
    bool assign_vertex_attrib_gles(gles_vertex_attrib &attrib, std::int32_t size, std::uint32_t type, std::int32_t stride, std::uint32_t offset, std::uint32_t buffer_obj);
    bool convert_gl_enum_to_primitive_mode(const std::uint32_t mode, drivers::graphics_primitive_mode &res);
    bool is_valid_gl_emu_func(const std::uint32_t func);
    std::uint32_t driver_blend_factor_to_gl_enum(const drivers::blend_factor factor);
}