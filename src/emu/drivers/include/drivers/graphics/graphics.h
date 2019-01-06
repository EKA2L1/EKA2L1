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

#include <drivers/driver.h>
#include <common/vecx.h>

#include <memory>

namespace eka2l1::drivers {
    enum graphics_driver_opcode {
        graphics_driver_clear,
        graphics_driver_resize_screen,
        graphics_driver_create_window,
        graphics_driver_destroy_window,
        graphics_driver_begin_window,
        graphics_driver_invalidate,
        graphics_driver_end_invalidate,
        graphics_driver_end_window,
        graphics_driver_set_brush_color,
        graphics_driver_set_window_size,
        graphics_driver_set_priority,
        graphics_driver_set_visibility,
        graphics_driver_set_win_pos,
        graphics_driver_draw_text_box
    };

    class graphics_driver: public driver {
    public:    
        graphics_driver() {}

        virtual vec2 get_screen_size() = 0;
        virtual void set_screen_size(const vec2 &s) = 0;
        virtual std::vector<std::uint8_t> get_render_texture_data(std::size_t stride) = 0;
        virtual std::uint32_t get_render_texture_handle() = 0;
    };

    using graphics_driver_ptr = std::shared_ptr<graphics_driver>;

    enum class graphic_api {
        opengl,
        vulkan
    };

    bool init_graphics_library(graphic_api api);
    
    graphics_driver_ptr create_graphics_driver(const graphic_api api, const vec2 &screen_size);
};