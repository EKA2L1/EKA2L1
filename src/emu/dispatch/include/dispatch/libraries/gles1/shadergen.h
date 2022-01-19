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

#include <dispatch/libraries/gles1/def.h>
#include <string>

namespace eka2l1::dispatch {
    std::string generate_gl_vertex_shader(const std::uint64_t vertex_statuses, const std::uint32_t active_texs, const bool is_es);
    std::string generate_gl_fragment_shader(const std::uint64_t fragment_statuses, const std::uint32_t active_texs,
        gles_texture_env_info *tex_env_infos, const bool is_es);
}