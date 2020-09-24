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

#include <common/types.h>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace eka2l1::common {
    using string_table = std::unordered_map<std::string, std::string>;

    /**
     * @brief Get localised string table from resource folder.
     * 
     * First, it looks for the localised folder contains string table. If it's not found, the source
     * folder is looked up.
     * 
     * @param resource_folder           The folder contains localised string folders.
     * @param table_file_name           Filename of string table.
     * @param lang                      Target language to get localised info.
     * 
     * @returns String table. Empty on failure.
     */
    string_table get_localised_string_table(const std::string &resource_folder, const std::string &table_file_name,
        const language lang);

    std::string get_localised_string(string_table &table, const std::string &key);
}