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
    class graphics_driver;

    class ogl_framebuffer : public framebuffer {
        std::uint32_t fbo;

        int last_fb{ 0 };

    public:
        std::uint32_t get_fbo() const {
            return fbo;
        }

        explicit ogl_framebuffer(texture_ptr &color_buffer, texture_ptr &depth_buffer);
        ~ogl_framebuffer() override;

        void bind(graphics_driver *driver) override;
        void unbind(graphics_driver *driver) override;

        std::uint64_t texture_handle() override;
    };
}
