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

namespace eka2l1::drivers {
    class graphics_driver;

    enum class buffer_hint {
        none = 0,
        vertex_buffer = 1,
        index_buffer = 2
    };

    enum buffer_upload_hint {
        buffer_upload_stream = 1 << 0,
        buffer_upload_static = 1 << 1,
        buffer_upload_dynamic = 1 << 2,

        buffer_upload_draw = 1 << 10,
        buffer_upload_read = 1 << 11,
        buffer_upload_copy = 1 << 12
    };

    enum buffer_data_format {
        byte = 0,
        sbyte = 1,
        word = 2,
        sword = 3,
        sfloat = 4,
        uint = 5,
        sint = 6
    };

    struct attribute_descriptor {
        int location;
        int offset;
        int format;
    };

    class buffer: public graphics_object {
    public:
        virtual ~buffer() = 0;

        virtual void bind(graphics_driver *driver) = 0;
        virtual void unbind(graphics_driver *driver) = 0;

        // Only support with buffer hinted as vertex
        virtual void attach_descriptors(graphics_driver *driver, const int stride, const bool instance_move, 
            const attribute_descriptor *descriptors, const int total) = 0;

        virtual bool create(graphics_driver *driver, const std::size_t initial_size, const buffer_hint hint, const buffer_upload_hint use_hint) = 0;
        virtual void update_data(graphics_driver *driver, const void *data, const std::size_t offset, const std::size_t size) = 0;
    };
}