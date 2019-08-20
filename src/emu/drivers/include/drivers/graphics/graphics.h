/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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
#include <drivers/driver.h>
#include <drivers/graphics/common.h>

#include <memory>

namespace eka2l1::drivers {
    enum graphics_driver_opcode : std::uint16_t {
        // Mode -1: Miscs
        graphics_driver_invalidate_rect,
        graphics_driver_set_invalidate,

        // Mode 0: Immediate - Draw direct 2D elements to screen
        graphics_driver_clear,
        graphics_driver_create_bitmap,
        graphics_driver_destroy_bitmap,
        graphics_driver_bind_bitmap,
        graphics_driver_set_brush_color,
        graphics_driver_draw_text_box,
        graphics_driver_update_bitmap,
        graphics_driver_draw_bitmap,
        graphics_driver_resize_bitmap

        // Mode 1: Advance - Lower access to functions
    };

    class graphics_driver : public driver {
        graphic_api api_;

    public:
        explicit graphics_driver(graphic_api api)
            : api_(api) {}

        const graphic_api get_current_api() const {
            return api_;
        }

        virtual void update_bitmap(drivers::handle h, const std::size_t size, const eka2l1::vec2 &offset,
            const eka2l1::vec2 &dim, const int bpp, const void *data) = 0;
    };

    using graphics_driver_ptr = std::shared_ptr<graphics_driver>;

    bool init_graphics_library(graphic_api api);

    graphics_driver_ptr create_graphics_driver(const graphic_api api);
};
