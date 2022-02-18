/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <services/drm/rights/object.h>

#include <sqlite3.h>
#include <string>

namespace eka2l1::epoc::drm {
    struct rights_database {
    private:
        sqlite3 *database_;
        sqlite3_stmt *get_cid_exist_stmt_;
        sqlite3_stmt *delete_record_stmt_;
        sqlite3_stmt *add_right_info_stmt_;
        sqlite3_stmt *add_perm_info_stmt_;
        sqlite3_stmt *add_constraint_info_stmt_;
        sqlite3_stmt *set_perm_constraints_stmt_;
        sqlite3_stmt *query_perms_stmt_;
        sqlite3_stmt *query_constraint_stmt_;
        sqlite3_stmt *get_encrypt_key_stmt_;
        sqlite3_stmt *get_cid_exist_full_stmt_;

        int add_constraint(const rights_constraint &constraint, const int perm_id);
        bool get_constraint(const int constraint_id, rights_constraint &result);
        bool get_permission_list(const int right_id, std::vector<rights_permission> &permissions);

    public:
        explicit rights_database(const std::u16string &database);
        ~rights_database();

        bool add_or_update_record(rights_object &obj);
        int get_entry_id(const std::string &content_id);

        bool get_record(const std::string &content_id, rights_object &result);
        bool get_encryption_key(const std::string &content_id, std::string &output_key);
        bool get_permission_list(const std::string &content_id, std::vector<rights_permission> &permissions);
    };
}