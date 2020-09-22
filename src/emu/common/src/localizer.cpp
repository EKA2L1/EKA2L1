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

#include <common/localizer.h>
#include <common/fileutils.h>
#include <common/path.h>

#include <fmt/format.h>
#include <pugixml.hpp>

namespace eka2l1::common {
    static const char *LANGUAGE_CODE_ARR[] = {
        #define LANG_DECL(x, y) #x,
        #include <common/lang.def>
        #undef LANG_DECL
    };

    static void fill_up_string_table_from_xml(pugi::xml_document &document, string_table &table) {
        pugi::xml_node resource_child = document.child("resources");

        if (!resource_child) {
            return;
        }

        for (auto &child: resource_child.children()) {
            if (std::strcmp(child.name(), "string") == 0) {
                pugi::xml_attribute name_attrib = child.attribute("name");

                if (!name_attrib) {
                    continue;
                }

                const std::string key = name_attrib.value();
                const std::string value = child.child_value();

                table[key] = value;
            }
        }
    }

    string_table get_localised_string_table(const std::string &resource_folder, const std::string &table_file_name, const language lang) {
        const std::string source_path = eka2l1::add_path(resource_folder, fmt::format("\\local\\{}", table_file_name));

        const std::string localised_path = eka2l1::add_path(resource_folder, fmt::format("\\local-{}\\strings.xml",
            LANGUAGE_CODE_ARR[static_cast<int>(lang)], table_file_name));

        string_table table;

        pugi::xml_document document;
        if (document.load_file(source_path.c_str()).status != pugi::xml_parse_status::status_ok) {
            return table;
        }
        
        fill_up_string_table_from_xml(document, table);
        document.reset();

        if (document.load_file(localised_path.c_str()).status != pugi::xml_parse_status::status_ok) {
            return table;
        }
        
        fill_up_string_table_from_xml(document, table);
        return table;
    }

    std::string get_localised_string(string_table &table, const std::string &key) {
        auto result = table.find(key);

        if (result == table.end()) {
            return key;
        }

        return result->second;
    }
}