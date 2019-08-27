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
    static GLenum get_trait_from_buffer_hint(const buffer_hint hint) {
        switch (hint) {
        case buffer_hint::index_buffer: {
            return GL_ELEMENT_ARRAY_BUFFER;
        }

        case buffer_hint::vertex_buffer: {
            return GL_ARRAY_BUFFER;
        }

        default:
            break;
        }

        return GL_INVALID_ENUM;
    }

    static GLenum get_binding_trait_from_hint(const buffer_hint hint) {
        switch (hint) {
        case buffer_hint::index_buffer:
            return GL_ELEMENT_ARRAY_BUFFER_BINDING;

        case buffer_hint::vertex_buffer:
            return GL_ARRAY_BUFFER_BINDING;

        default:
            break;
        }

        return GL_INVALID_ENUM;
    }

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

    static void data_format_to_gl_comp_count_and_data_type(const int format, int &comp_count, GLenum &gl_format) {
        comp_count = (format & 0b1111);
        const data_format our_format = static_cast<data_format>((format >> 4) & 0b1111);
        gl_format = data_format_to_gl_enum(our_format);
    }

    ogl_buffer::ogl_buffer()
        : buffer_(0)
        , descriptor_(0)
        , hint_(buffer_hint::none)
        , size_(0) {
        hint_gl_ = get_trait_from_buffer_hint(hint_);
    }

    ogl_buffer::~ogl_buffer() {
        if (buffer_ != 0) {
            glDeleteBuffers(1, &buffer_);
        }

        if (descriptor_ != 0) {
            glDeleteVertexArrays(1, &descriptor_);
        }
    }

    void ogl_buffer::bind(graphics_driver *driver) {
        if (descriptor_) {
            glBindVertexArray(descriptor_);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, reinterpret_cast<GLint *>(&last_descriptor_));
        }

        if (buffer_) {
            glBindBuffer(hint_gl_, buffer_);
            glGetIntegerv(get_binding_trait_from_hint(hint_), reinterpret_cast<GLint *>(&last_buffer_));
        }
    }

    void ogl_buffer::unbind(graphics_driver *driver) {
        if (descriptor_) {
            glBindVertexArray(last_descriptor_);
        }

        if (buffer_) {
            glBindBuffer(hint_gl_, last_buffer_);
        }
    }

    bool ogl_buffer::create(graphics_driver *driver, const std::size_t initial_size, const buffer_hint hint, const buffer_upload_hint use_hint) {
        hint_ = hint;
        hint_gl_ = get_trait_from_buffer_hint(hint);
        usage_hint_gl_ = get_usage_hint(use_hint);

        glCreateBuffers(1, &buffer_);

        // Prealloc data first
        bind(driver);
        glBufferData(hint_gl_, initial_size, nullptr, usage_hint_gl_);
        unbind(driver);

        size_ = initial_size;

        return true;
    }

    void ogl_buffer::update_data(graphics_driver *driver, const void *data, const std::size_t offset, std::size_t size) {
        bind(driver);

        if (offset + size > size_) {
            if (offset == 0) {
                // Orphan the buffer
                glBufferData(hint_gl_, size, nullptr, usage_hint_gl_);
            }
        }

        // Use subdata to update
        glBufferSubData(hint_gl_, static_cast<GLintptr>(offset), static_cast<GLintptr>(size), data);

        unbind(driver);
    }

    void ogl_buffer::attach_descriptors(graphics_driver *driver, const int stride, const bool instance_move,
        const attribute_descriptor *descriptors, const int total) {
        if (hint_ != buffer_hint::vertex_buffer) {
            return;
        }

        if (!descriptor_) {
            glGenVertexArrays(1, &descriptor_);
        }

        bind(driver);

        // We need to translate descriptors!!!
        for (int i = 0; i < total; i++) {
            const attribute_descriptor &descriptor = descriptors[i];
            int comp_count = 1;
            GLenum data_type = GL_INVALID_ENUM;

            data_format_to_gl_comp_count_and_data_type(descriptor.format, comp_count, data_type);

            glEnableVertexArrayAttrib(descriptor_, descriptor.location);
            glVertexAttribPointer(descriptor.location, comp_count, data_type, false, stride, (GLvoid *)descriptor.offset);
        }

        unbind(driver);
    }
}