/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <common/rgb.h>
#include <common/vecx.h>

namespace eka2l1::common::color {
    using vec_rgb = vecx<int, 3>;
    #define COLOR_DECL(name, r, g, b) const vec_rgb name = vec_rgb({ r, g, b });

    #include <common/color.def>

    #undef COLOR_DECL

    const vec_rgb get_color(const char *name);
}
