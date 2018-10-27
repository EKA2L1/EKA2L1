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

#include <cstdint>
#include <string>
#include <optional>

namespace eka2l1::drivers {
    class shader {
    public:
        virtual bool create(const char *vert_data, const std::size_t vert_size,
            const char *frag_data, const std::size_t frag_size) = 0;

        virtual bool use() = 0;
        virtual std::optional<int> get_uniform_location(const std::string &name) = 0;
        virtual std::optional<int> get_attrib_location(const std::string &name) = 0;
    };
}