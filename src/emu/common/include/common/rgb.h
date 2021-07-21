/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <common/vecx.h>
#include <cstdint>

namespace eka2l1::common {
    inline vecx<std::uint8_t, 4> rgba_to_vec(const std::uint32_t rgba) {
        return vecx<std::uint8_t, 4>{ static_cast<std::uint8_t>(rgba & 0x000000FF),
            static_cast<std::uint8_t>((rgba & 0x0000FF00) >> 8),
            static_cast<std::uint8_t>((rgba & 0x00FF0000) >> 16),
            static_cast<std::uint8_t>(rgba >> 24) };
    }

    using rgba = std::uint32_t;
}
