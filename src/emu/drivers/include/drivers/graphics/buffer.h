/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <cstddef>
#include <drivers/graphics/common.h>
#include <memory>

namespace eka2l1::drivers {
    class graphics_driver;

    enum buffer_upload_hint : std::uint16_t {
        buffer_upload_stream = 1 << 0,
        buffer_upload_static = 1 << 1,
        buffer_upload_dynamic = 1 << 2,

        buffer_upload_draw = 1 << 10,
        buffer_upload_read = 1 << 11,
        buffer_upload_copy = 1 << 12
    };

    class buffer : public graphics_object {
    public:
        virtual ~buffer(){};

        virtual void bind(graphics_driver *driver) = 0;
        virtual void unbind(graphics_driver *driver) = 0;

        virtual bool create(graphics_driver *driver, const void *data, const std::size_t initial_size, const buffer_upload_hint use_hint) = 0;
        virtual void update_data(graphics_driver *driver, const void *data, const std::size_t offset, const std::size_t size) = 0;
    };

    std::unique_ptr<buffer> make_buffer(graphics_driver *driver);
}