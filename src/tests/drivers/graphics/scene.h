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

#include "spritebatch.h"

#include <drivers/graphics/common.h>
#include <drivers/graphics/graphics.h>

#include <common/bitmap.h>

using namespace eka2l1;

enum scene_flags {
    init = 0x1
};

struct scene {
    std::uint32_t         flags {0};
};

struct bitmap_draw_scene: public scene {
    common::bmp_header    bmp_header;
    common::dib_header_v1 dib_header;

    drivers::handle tex;
};

struct scenes {
    drivers::graphic_api gr_api;
    drivers::graphics_driver_ptr driver;
    sprite_batch_inst spritebatch;

    bitmap_draw_scene bmp_draw_scene;
};
