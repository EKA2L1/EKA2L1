/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <services/etel/common.h>

#include <optional>
#include <string>
#include <vector>

namespace eka2l1 {
    class io_system;
}

namespace eka2l1::epoc::etel {
    class module_manager {
        std::vector<std::string> loaded_;
        std::vector<etel_module_entry> entries_;

    public:
        explicit module_manager();

        bool load_tsy(io_system *io, const std::string &module_name);
        std::optional<std::uint32_t> get_entry_real_index(const std::uint32_t respective_index, const etel_entry_type type);

        bool get_entry(const std::uint32_t real_index, etel_module_entry **entry);
        bool get_entry_by_name(const std::string &name, etel_module_entry **entry);

        std::size_t total_entries(const etel_entry_type type) const;
    };
}