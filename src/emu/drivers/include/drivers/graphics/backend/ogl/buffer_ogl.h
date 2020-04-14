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
#include <drivers/graphics/buffer.h>

#include <glad/glad.h>

namespace eka2l1::drivers {
    class ogl_buffer : public buffer {
        GLuint buffer_;
        GLuint descriptor_;

        GLuint last_buffer_;
        GLuint last_descriptor_;

        buffer_hint hint_;
        GLenum hint_gl_;
        GLenum usage_hint_gl_;

        std::size_t size_;

    public:
        explicit ogl_buffer();
        ~ogl_buffer() override;

        void bind(graphics_driver *driver) override;
        void unbind(graphics_driver *driver) override;

        void attach_descriptors(graphics_driver *driver, const int stride, const bool instance_move,
            const attribute_descriptor *descriptors, const int total) override;

        bool create(graphics_driver *driver, const std::size_t initial_size, const buffer_hint hint, const buffer_upload_hint use_hint) override;
        void update_data(graphics_driver *driver, const void *data, const std::size_t offset, const std::size_t size) override;
    };
}