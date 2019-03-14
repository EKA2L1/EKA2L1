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

#include "../spritebatch.h"

#include <drivers/graphics/backend/ogl/shader_ogl.h>
#include <drivers/graphics/backend/ogl/texture_ogl.h>

#include <glad/glad.h>

using namespace eka2l1;

class sprite_batch_ogl: public sprite_batch {
    drivers::ogl_shader shader_;
    GLuint quad_vao;

public:
    sprite_batch_ogl();
    void draw(const drivers::handle sprite, const eka2l1::vec2 &pos, const eka2l1::vec2 &size,
        const float rotation = 0.0f, const eka2l1::vec3 &color = eka2l1::vec3(1, 1, 1)) override;
};
