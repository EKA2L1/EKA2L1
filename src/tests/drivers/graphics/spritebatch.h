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

#include <drivers/graphics/texture.h>
#include <drivers/graphics/common.h>

#include <memory>

using namespace eka2l1;

class sprite_batch {
public:
    virtual void draw(const drivers::handle sprite, const eka2l1::vec2 &pos, const eka2l1::vec2 &size,
        const float rotation = 0.0f, const eka2l1::vec3 &color = eka2l1::vec3(1, 1, 1)) = 0;
};

using sprite_batch_inst = std::unique_ptr<sprite_batch>;

sprite_batch_inst create_sprite_batch(const drivers::graphic_api gr_api);
