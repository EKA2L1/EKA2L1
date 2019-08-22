/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <common/vecx.h>
#include <drivers/graphics/common.h>

#include <cstddef>
#include <cstdint>
#include <memory>

struct ImDrawData;

namespace eka2l1::drivers {
    class graphics_driver;

    class imgui_renderer {
        drivers::handle shader;
        drivers::handle font_texture;

    public:
        void init(graphics_driver *driver);
        virtual void render(ImDrawData *draw_data) = 0;
        virtual void deinit() = 0;

        virtual void draw(drivers::handle h, const eka2l1::rect &r);
    };
}
