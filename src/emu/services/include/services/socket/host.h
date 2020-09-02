/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1::epoc::socket {
    struct host;

    struct connection {
        host *parent_;
        std::u16string name_;
    };

    struct host {
        std::uint32_t protocol_;
        std::uint32_t family_;

        std::u16string name_;
        std::vector<connection*> conns_;

        std::uint64_t hash_lookup() const {
            return static_cast<std::uint64_t>(protocol_) | (static_cast<std::uint64_t>(family_) << 32);
        }
    };
}