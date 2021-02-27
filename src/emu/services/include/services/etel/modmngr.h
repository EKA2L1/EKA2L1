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

    namespace kernel {
        using uid = std::uint64_t;
    }
}

namespace eka2l1::epoc::etel {
    class module_manager {
        struct tsy_module_info {
            std::string name_;
            std::vector<kernel::uid> used_sessions_;
        };

        std::vector<tsy_module_info> loaded_;
        std::vector<etel_module_entry> entries_;

    protected:
        std::string get_full_tsy_path(io_system *io, const std::string &module_name);

    public:
        explicit module_manager();

        bool load_tsy(io_system *io, const kernel::uid borrowed_session, const std::string &module_name);
        bool close_tsy(io_system *io, const kernel::uid borrowed_session, const std::string &module_name);

        void unload_from_sessions(io_system *io, const kernel::uid borrowed_session);
        
        std::optional<std::uint32_t> get_entry_real_index(const std::uint32_t respective_index, const etel_entry_type type);

        bool get_entry(const std::uint32_t real_index, etel_module_entry **entry);
        bool get_entry_by_name(const std::string &name, etel_module_entry **entry);

        std::size_t total_entries(const etel_entry_type type) const;
    };
}