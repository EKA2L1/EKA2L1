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

#include <common/types.h>
#include <epoc/utils/uid.h>

#include <cstdint>
#include <string>
#include <optional>

namespace eka2l1 {
    class io_system;
}

namespace eka2l1::epoc::msv {
    struct entry {
        std::uint32_t id_;
        std::int32_t parent_id_;
        std::uint32_t service_id_;
        epoc::uid type_uid_;
        epoc::uid mtm_uid_;
        std::uint32_t data_;
        std::u16string description_;
        std::u16string details_;
        std::uint64_t time_;

        enum flag {
            STATUS_PRESENT = 1 << 0,
            STATUS_DELETED = 1 << 1,
            STATUS_CORRUPTED = 1 << 2
        };
        
        std::uint32_t flags_;
    };

    struct entry_indexer {
        std::vector<entry> entries_;

        io_system *io_;
        drive_number rom_drv_;
        language preferred_lang_;
        std::u16string msg_dir_;

    protected:
        bool create_standard_entries(drive_number crr_drive);
        bool create_root_entry();

        bool update_entries_file(const std::size_t entry_pos_start);
        bool load_entries_file(drive_number crr_drive);

        std::optional<std::u16string> get_entry_data_file(entry &ent);
        
        /**
         * \brief       Save this entry body data to correspond store.
         * \param       ent   The entry to save.
         * 
         * \returns     True on success.
         */
        bool save_entry_data(entry &ent);

        /**
         * \brief       Load this entry body data from correspond store.
         * \param       ent The entry to load. ID and parent ID must be presented/
         * 
         * \returns     True on success.
         */
        bool load_entry_data(entry &ent);

    public:
        explicit entry_indexer(io_system *io, const std::u16string &msg_folder, const language preferred_lang);
        bool add_entry(entry &ent);

        entry *get_entry(const std::uint32_t id);
        std::vector<entry*> get_entries_by_parent(const std::uint32_t parent_id);
    };
}