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

#pragma once

#include <services/fs/fs.h>

namespace eka2l1 {

    enum file_parse_type {
        file_parse_drive,
        file_parse_path,
        file_parse_name,
        file_parse_ext
    };

    class file_parser {
        std::u16string name_;
        std::u16string related_;
        std::u16string result;
        file_parse parse_;
        bool done;

        void parse_part(std::uint32_t type, std::u16string name);
        void parse_drive(std::u16string name);
        void parse_path(std::u16string name);
        void parse_name(std::u16string name);
        void parse_ext(std::u16string name);

    public:
        file_parser(std::u16string name, std::u16string related, file_parse parse);

        void parse(std::u16string default_path);
        file_parse get_result();
    };
}
