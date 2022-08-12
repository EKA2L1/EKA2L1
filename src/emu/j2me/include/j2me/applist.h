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

#include <string>
#include <vector>
#include <optional>

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

namespace eka2l1::config {
    struct state;
}

namespace eka2l1::j2me {
    static const char *APPLIST_DB_NAME = "j2me\\applist.db";
    static const char *APPLIST_DB_DIR = "j2me\\";

    struct app_entry {
        std::uint32_t id_;
        std::string name_;
        std::string author_;
        std::string icon_path_;
        std::string version_;
        std::string title_;
        std::string original_title_;
    };

    class app_list {
    private:
        sqlite3 *database_;
        sqlite3_stmt *find_existing_stmt_;
        sqlite3_stmt *insert_entry_stmt_;
        sqlite3_stmt *update_name_stmt_;
        sqlite3_stmt *update_entry_stmt_;
        sqlite3_stmt *delete_entry_stmt_;
        sqlite3_stmt *version_query_stmt_;
        sqlite3_stmt *individual_select_stmt_;

        const config::state &conf_;

    private:
        void create_and_initialize_database(const config::state &conf);
        bool execute_insert(const app_entry &entry);
        void reset();

        std::uint32_t update_entry(const app_entry &entry);

    public:
        explicit app_list(const config::state &conf);
        ~app_list();

        std::uint32_t add_entry(const app_entry &entry, const bool replace_existing);
        void add_entries(const std::vector<app_entry> &entries);

        bool remove_entry(const std::uint32_t id);
        bool update_name(const std::uint32_t id, const std::string &new_name);

        std::uint32_t get_launching_version();
        void update_launching_version();

        std::vector<app_entry> get_entries();
        std::optional<app_entry> get_entry(const std::uint32_t id);

        void flush();
    };
}