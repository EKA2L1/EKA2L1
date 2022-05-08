/*
 * Copyright (c) 2022 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <drivers/graphics/backend/ogl/common_ogl.h>
#include <drivers/graphics/backend/ogl/input_desc_ogl.h>
#include <glad/glad.h>

#include <cstring>

namespace eka2l1::drivers {
    input_descriptors_ogl::input_descriptors_ogl()
        : vao_(0) {
        for (int i = 0; i < GL_BACKEND_MAX_VBO_SLOTS; i++) {
            cached_vbo_[i] = 0;
        }
    }

    input_descriptors_ogl::~input_descriptors_ogl() {
        if (vao_) {
            glDeleteVertexArrays(1, &vao_);
        }
    }

    bool input_descriptors_ogl::modify(drivers::graphics_driver *drv, input_descriptor *descs, const int count) {
        inputs_.clear();

        for (int i = 0; i < count; i++) {
            if (descs[i].buffer_slot > GL_BACKEND_MAX_VBO_SLOTS) {
                return false;
            }

            inputs_.push_back(*reinterpret_cast<input_descriptor_ogl*>(descs + i));
        }

        for (int i = 0; i < GL_BACKEND_MAX_VBO_SLOTS; i++) {
            cached_vbo_[i] = 0;
        }

        if (!vao_) {
            glGenVertexArrays(1, &vao_);
        }

        return true;
    }

    static void data_format_to_gl_comp_count_and_data_type(const int format, int &comp_count, GLenum &gl_format) {
        comp_count = (format & 0b1111);
        const data_format our_format = static_cast<data_format>((format >> 4) & 0b1111);
        gl_format = data_format_to_gl_enum(our_format);
    }

    void input_descriptors_ogl::activate(std::uint32_t *current_vbo_bindings) {
        bool cache_change = false;

        glBindVertexArray(vao_);

        for (std::size_t i = 0; i < inputs_.size(); i++) {
            int buffer_slot_cur = inputs_[i].buffer_slot;

            if (!inputs_[i].is_updated() || (current_vbo_bindings[buffer_slot_cur] != cached_vbo_[buffer_slot_cur])) {
                int comp_count = 1;
                GLenum data_type = GL_INVALID_ENUM;

                data_format_to_gl_comp_count_and_data_type(inputs_[i].format, comp_count, data_type);

                glBindBuffer(GL_ARRAY_BUFFER, current_vbo_bindings[buffer_slot_cur]);
                glEnableVertexAttribArray(inputs_[i].location);

                std::uint64_t offset_descriptor = inputs_[i].offset;
                glVertexAttribPointer(inputs_[i].location, comp_count, data_type, inputs_[i].is_normalized(), inputs_[i].stride, (GLvoid *)offset_descriptor);
                
                if (inputs_[i].is_per_instance()) {
                    glVertexAttribDivisor(inputs_[i].location, 1);
                } else {
                    glVertexAttribDivisor(inputs_[i].location, 0);
                }

                inputs_[i].set_updated(true);
                cache_change = true;
            }
        }

        if (cache_change) {
            std::memcpy(cached_vbo_, current_vbo_bindings, GL_BACKEND_MAX_VBO_SLOTS * sizeof(std::uint32_t));
        }
    }
}