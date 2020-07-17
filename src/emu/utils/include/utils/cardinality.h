/*
 * Copyright (c) 2020 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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

#include <cstdint>

namespace eka2l1::common {
    class ro_stream;
}

namespace eka2l1::utils {
    struct cardinality {
        std::uint32_t val_;

        explicit cardinality(const std::uint32_t raw_val = 0)
            : val_(raw_val) {
        }

        std::uint32_t length() const {
            return val_ >> 1;
        }

        std::uint32_t raw_value() const {
            return val_;
        }

        bool internalize(common::ro_stream &stream);
    };
}