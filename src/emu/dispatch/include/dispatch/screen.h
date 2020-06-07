/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <common/vecx.h>
#include <dispatch/def.h>

#include <mem/ptr.h>

namespace eka2l1::dispatch {
    struct fast_blit_info {
        eka2l1::ptr<std::uint8_t> dest_base;
        eka2l1::ptr<const std::uint8_t> src_base;
        eka2l1::vec2 dest_point;
        std::uint32_t dest_stride;
        std::uint32_t src_stride;
        eka2l1::vec2 src_size;
        eka2l1::rect src_blit_rect;
    };

    BRIDGE_FUNC_DISPATCHER(void, update_screen, const std::uint32_t screen_number, const std::uint32_t num_rects, const eka2l1::rect *rect_list);
    BRIDGE_FUNC_DISPATCHER(void, fast_blit, fast_blit_info *info);
}