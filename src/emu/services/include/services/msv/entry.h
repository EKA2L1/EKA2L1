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

#include <services/msv/common.h>
#include <services/msv/cache.h>

#include <sqlite3.h>

#include <cstdint>
#include <optional>
#include <string>

namespace eka2l1 {
    class io_system;
}

namespace eka2l1::epoc::msv {
    struct entry_indexer {
    protected:
        io_system *io_;
        drive_number rom_drv_;
        language preferred_lang_;
        std::u16string msg_dir_;

        common::roundabout folders_;
        entry root_entry_;

        bool create_standard_entries(drive_number crr_drive);
        std::vector<entry *> get_entries_by_parent_through_cache(const std::uint32_t visible_folder, const std::uint32_t parent_id);

    public:
        explicit entry_indexer(io_system *io, const std::u16string &msg_folder, const language preferred_lang);
        virtual ~entry_indexer();

        virtual entry *add_entry(entry &ent);
        virtual entry *get_entry(const std::uint32_t id);
        virtual bool change_entry(entry &ent);

        virtual std::vector<entry *> get_entries_by_parent(const std::uint32_t parent_id) = 0;
        std::optional<std::u16string> get_entry_data_file(entry &ent);
    };

    struct sql_entry_indexer: public entry_indexer {
    private:
        sqlite3 *database_;
        sqlite3_stmt *create_entry_stmt_;
        sqlite3_stmt *change_entry_stmt_;
        sqlite3_stmt *visible_folder_find_stmt_;
        sqlite3_stmt *find_entry_stmt_;
        sqlite3_stmt *query_child_entries_stmt_;

        std::uint32_t id_counter_;

        bool load_or_create_databases(bool &newly_created);
        bool collect_children_entries(const msv_id parent_id, std::vector<entry> &entries);
        void fill_entry_information(entry &ent, sqlite3_stmt *stmt, const bool have_extra_id = false);

        msv_id get_suitable_visible_parent_id(const msv_id parent_id);
        bool add_or_change_entry(entry &ent, entry *&result, const bool is_add);

    public:
        explicit sql_entry_indexer(io_system *io, const std::u16string &msg_folder, const language preferred_lang);
        ~sql_entry_indexer() override;
        
        entry *add_entry(entry &ent) override;
        entry *get_entry(const std::uint32_t id) override;
        bool change_entry(entry &ent) override;

        std::vector<entry *> get_entries_by_parent(const std::uint32_t parent_id) override;
    };
}