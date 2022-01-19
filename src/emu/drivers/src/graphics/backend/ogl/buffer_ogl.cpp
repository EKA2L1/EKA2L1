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

#include <drivers/graphics/backend/ogl/buffer_ogl.h>
#include <drivers/graphics/backend/ogl/common_ogl.h>

namespace eka2l1::drivers {
    static GLenum get_usage_hint(const buffer_upload_hint usage_hint) {
        if (usage_hint & buffer_upload_static) {
            if (usage_hint & buffer_upload_draw) {
                return GL_STATIC_DRAW;
            }

            if (usage_hint & buffer_upload_copy) {
                return GL_STATIC_COPY;
            }

            if (usage_hint & buffer_upload_read) {
                return GL_STATIC_READ;
            }

            return GL_INVALID_ENUM;
        }

        if (usage_hint & buffer_upload_dynamic) {
            if (usage_hint & buffer_upload_draw) {
                return GL_DYNAMIC_DRAW;
            }

            if (usage_hint & buffer_upload_copy) {
                return GL_DYNAMIC_COPY;
            }

            if (usage_hint & buffer_upload_read) {
                return GL_DYNAMIC_READ;
            }

            return GL_INVALID_ENUM;
        }

        if (usage_hint & buffer_upload_stream) {
            if (usage_hint & buffer_upload_draw) {
                return GL_STREAM_DRAW;
            }

            if (usage_hint & buffer_upload_copy) {
                return GL_STREAM_COPY;
            }

            if (usage_hint & buffer_upload_read) {
                return GL_STREAM_READ;
            }

            return GL_INVALID_ENUM;
        }

        return GL_INVALID_ENUM;
    }

    ogl_buffer::ogl_buffer()
        : buffer_(0)
        , size_(0) {
    }

    ogl_buffer::~ogl_buffer() {
        if (buffer_ != 0) {
            glDeleteBuffers(1, &buffer_);
        }
    }

    void ogl_buffer::bind(graphics_driver *driver) {
        if (buffer_) {
            glBindBuffer(GL_ARRAY_BUFFER, buffer_);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, reinterpret_cast<GLint *>(&last_buffer_));
        }
    }

    void ogl_buffer::unbind(graphics_driver *driver) {
        if (buffer_) {
            glBindBuffer(GL_ARRAY_BUFFER, last_buffer_);
        }
    }

    bool ogl_buffer::create(graphics_driver *driver, const void *data, const std::size_t initial_size, const buffer_upload_hint use_hint) {
        usage_hint_gl_ = get_usage_hint(use_hint);

        if (!buffer_) {
            glGenBuffers(1, &buffer_);
        }

        // Prealloc data first
        bind(driver);
        glBufferData(GL_ARRAY_BUFFER, initial_size, data, usage_hint_gl_);
        unbind(driver);

        size_ = initial_size;

        return true;
    }

    void ogl_buffer::update_data(graphics_driver *driver, const void *data, const std::size_t offset, std::size_t size) {
        bind(driver);

        if (offset + size > size_) {
            if (offset == 0) {
                // Orphan the buffer
                glBufferData(GL_ARRAY_BUFFER, offset + size, nullptr, usage_hint_gl_);
            }

            size_ = offset + size;
        }

        // Use subdata to update
        glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offset), static_cast<GLintptr>(size), data);

        unbind(driver);
    }
}