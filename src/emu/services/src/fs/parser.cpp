/*
 * Copyright (c) 2020 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
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

#include <services/fs/parser.h>
#include <common/path.h>

#include <algorithm>

namespace eka2l1 {
    file_parser::file_parser(std::u16string name, std::u16string related, file_parse parse) 
        : name_(name)
        , related_(related)
        , parse_(parse) {
    }

    void file_parser::parse(std::u16string default_path) {
        memset(parse_.fields, 0, sizeof(parse_.fields));
        
        if (name_.find('?') != std::string::npos) {
            parse_.wild |= file_wild::file_wild_name | file_wild::file_wild_either | file_wild::file_wild_is_kmatch_one;
            name_.erase(std::remove(name_.begin(), name_.end(), '?'), name_.end());
        }
        if (name_.find('*') != std::string::npos) {
            parse_.wild |= file_wild::file_wild_name | file_wild::file_wild_either | file_wild::file_wild_is_kmatch_any;
            name_.erase(std::remove(name_.begin(), name_.end(), '*'), name_.end());
        }

        std::u16string lex[3];
        lex[0] = name_;
        lex[1] = related_;
        lex[2] = default_path;

        std::int32_t pos = 0;
        for (auto i = 0; i < LEX_COMPONENTS; i++) {
            done = false;
            for (auto j = 0; j < 3; j++) {
                parse_part(i, lex[j]);

                if (j == 0 && done) {
                    parse_.fields[i].present = true;
                }
            }

            std::int32_t len = static_cast<std::int32_t>(result.length()) - pos;
            parse_.fields[i].len = len;
            parse_.fields[i].pos = pos;
            pos += len;
        }

        parse_.name_buf = result;
    }

    void file_parser::parse_part(std::uint32_t type, std::u16string name) {
        switch (type) {
        case file_parse_type::file_parse_drive:
            parse_drive(name);
            break;

        case file_parse_type::file_parse_path:
            parse_path(name);
            break;

        case file_parse_type::file_parse_name:
            parse_name(name);
            break;

        case file_parse_type::file_parse_ext:
            parse_ext(name);
            break;

        default:
            break;
        }
    }

    void file_parser::parse_drive(std::u16string name) {
        if (name.length() < 3 || name.at(2) != '\\') {
            return;
        }
        if (!done) {
            result += name.substr(0, 3);
            done = true;
        }
    }

    void file_parser::parse_path(std::u16string name) {
        if (name.find('\\') == std::string::npos) {
            return;
        }
        std::size_t pos_start = 0;
        if (name.length() >= 3 && name.at(2) == '\\') {
            pos_start = 3;
        }
        std::size_t pos_end = name.find_last_of('\\');
        if (!done) {
            result += name.substr(pos_start, pos_end);
            done = true;
        }
    }

    void file_parser::parse_name(std::u16string name) {
        std::u16string fname = eka2l1::filename(name, true); 
        std::size_t pos_end = name.find_last_of('.');
        if (pos_end != std::string::npos) {
            fname = fname.substr(0, pos_end);
        }
        if (!done) {
            result += fname;
            done = true;
        }
    }

    void file_parser::parse_ext(std::u16string name) {
        std::u16string ext = eka2l1::path_extension(name);
        if (ext.length() == 0) {
            return;
        }
        if (!done) {
            result += ext;
            done = true;
        }
    }

    file_parse file_parser::get_result() {
        return parse_;
    }

}
