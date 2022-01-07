/*
 * Copyright (c) 202 EKA2L1 Team.
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

#pragma once

#include <drivers/graphics/backend/ogl/common_ogl.h>
#include <drivers/graphics/input_desc.h>
#include <vector>

namespace eka2l1::drivers {
    struct input_descriptor_ogl : public input_descriptor {
        bool is_updated() const {
            return format >> 12;
        }

        void set_updated(const bool rr) {
            format &= ~(1 << 12);
            if (rr) format |= (1 << 12);
        }
    };

    class input_descriptors_ogl : public input_descriptors {
    private:
        std::vector<input_descriptor_ogl> inputs_;

        std::uint32_t vao_;
        std::uint32_t cached_vbo_[GL_BACKEND_MAX_VBO_SLOTS];
    
    public:
        explicit input_descriptors_ogl();
        ~input_descriptors_ogl() override;

        bool modify(drivers::graphics_driver *drv, input_descriptor *descs, const int count) override;
        void activate(std::uint32_t *current_vbo_bindings);
    };

    std::unique_ptr<input_descriptors> make_input_descriptors(graphics_driver *driver);
}