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

#pragma once

#include <cstddef>
#include <drivers/graphics/common.h>
#include <memory>

namespace eka2l1::drivers {
    class graphics_driver;

    struct input_descriptor {
        int location;
        int offset;
        int format = 0;
        int stride;
        std::uint32_t buffer_slot;

        void set_format(const int comp_count, const data_format dform) {
            format &= ~0b111111111111;
            format |= (comp_count & 0b1111) | (static_cast<int>(dform) << 4);
        }

        void set_normalized(const bool norm) {
            format &= ~(1 << 12);
            if (norm) format |= 1 << 12;
        }

        bool is_normalized() const {
            return format & (1 << 12);
        }

        void set_per_instance(const bool per_inst) {
            format &= ~(1 << 13);
            if (per_inst) format |= (1 << 13);
        }

        bool is_per_instance() const {
            return format & (1 << 13);
        }
    };

    class input_descriptors : public graphics_object {
    public:
        virtual ~input_descriptors() = default;
        virtual bool modify(drivers::graphics_driver *drv, input_descriptor *descs, const int count) = 0;
    };

    std::unique_ptr<input_descriptors> make_input_descriptors(graphics_driver *driver);
}