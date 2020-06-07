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

#include <utils/consts.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace eka2l1 {
    class io_system;
}

namespace eka2l1::epoc::msv {
    struct mtm_component {
        std::uint32_t group_idx_;

        std::u16string name_;
        std::u16string filename_;
        std::uint32_t comp_uid_;
        std::uint32_t specific_uid_;
        std::uint16_t entry_point_;
        std::uint16_t major_;
        std::uint16_t minor_;
        std::uint16_t build_;

        mtm_component *next_;
    };

    struct mtm_group {
        std::uint32_t mtm_uid_;
        std::uint32_t tech_type_uid_;

        std::uint8_t cap_avail_;
        std::uint8_t cap_send_;
        std::uint8_t cap_body_;

        mtm_component comps_;

        std::uint32_t ref_count_;
    };

    class mtm_registry {
        std::vector<mtm_group> groups_;
        std::map<std::uint32_t, std::vector<mtm_component *>> comps_;
        io_system *io_;

        std::u16string list_path_;
        std::vector<std::u16string> mtm_files_;

    protected:
        void add_entry_to_mtm_list(const std::u16string &path);
        bool install_group_from_rsc(const std::u16string &path);

    public:
        explicit mtm_registry(io_system *io);

        void load_mtm_list();
        void save_mtm_list();

        bool install_group(const std::u16string &path);

        mtm_group *query_mtm_group(const epoc::uid the_uid);
        std::vector<mtm_component *> &get_components(const epoc::uid the_uid);

        mtm_group *get_group(const std::uint32_t idx) {
            if (groups_.size() <= idx) {
                return nullptr;
            }

            return &groups_[idx];
        }
    };
}