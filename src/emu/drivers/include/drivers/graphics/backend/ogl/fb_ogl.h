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

#include <drivers/graphics/backend/ogl/texture_ogl.h>
#include <drivers/graphics/fb.h>

#include <common/vecx.h>

#include <cstdint>

namespace eka2l1::drivers {
    class ogl_framebuffer : public framebuffer {
        std::uint32_t fbo;
        std::uint32_t rbo;

        ogl_texture texture;

    public:
        std::uint32_t get_fbo() const {
            return fbo;
        }

        explicit ogl_framebuffer(const vec2 &size);
        ~ogl_framebuffer() override;

        void bind() override;
        void unbind() override;

        void resize(const vec2 &size) override;

        std::vector<std::uint8_t> data(std::size_t stride_pixels) override;
        std::uint32_t texture_handle() override;
    };
}