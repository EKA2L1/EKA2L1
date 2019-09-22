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

#include <common/resource.h>
#include <common/vecx.h>

#include <drivers/graphics/common.h>

#include <memory>
#include <vector>

namespace eka2l1::drivers {
    class graphics_driver;
    class texture;

    class framebuffer: public graphics_object {
    protected:
        texture *color_buffer;
        texture *depth_buffer;

    public:
        explicit framebuffer(texture *color_buffer, texture *depth_buffer)
            : color_buffer(color_buffer)
            , depth_buffer(depth_buffer) {
        }

        framebuffer() = default;
        virtual ~framebuffer(){};

        virtual void bind(graphics_driver *driver) = 0;
        virtual void unbind(graphics_driver *driver) = 0;

        virtual std::uint64_t texture_handle() = 0;
    };

    using framebuffer_ptr = std::unique_ptr<framebuffer>;

    framebuffer_ptr make_framebuffer(graphics_driver *driver, texture *color_buffer, texture *depth_buffer);
}
