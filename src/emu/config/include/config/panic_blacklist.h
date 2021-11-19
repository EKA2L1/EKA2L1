/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <unordered_map>

namespace eka2l1::config {
    struct panic_block_info {
        std::string process_name_;
        std::string thread_name_;
        std::string category_;
        std::int32_t code_;
    };

    class panic_blacklist {
    private:
        std::unordered_multimap<std::string, panic_block_info> blacklist_;

    public:
        explicit panic_blacklist();

        bool should_be_blocked(const std::string &process_name, const std::string &thread_name,
            const std::string &category, const std::int32_t code);
    };
}