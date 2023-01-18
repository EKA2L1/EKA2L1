/*
 * Copyright (c) 2023 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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

#include <cstdint>

namespace eka2l1::utils {
    struct akn_icon_header {
        std::uint16_t sign_;
        std::uint8_t version_;
        std::uint8_t header_size_;
        std::uint32_t reserved_1_;
        std::uint16_t reserved_2_;
        std::uint8_t reserved_3_;
        std::uint8_t reserved_4_ : 4; 
        std::uint8_t is_margin_correction_ : 1;
        std::uint8_t is_mask_ : 1;
        std::uint8_t aspect_ratio_ : 2;
        std::int32_t rotation_;
        std::int32_t icon_color_;
        std::int32_t bitmap_id_;
    };
}