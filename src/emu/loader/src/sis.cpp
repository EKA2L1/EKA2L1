/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <loader/sis.h>
#include <loader/sis_common.h>
#include <loader/sis_fields.h>

#include <common/buffer.h>

#include <cassert>
#include <cstdio>

#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

namespace eka2l1 {
    namespace loader {
        enum {
            uid1_cst = 0x10201A7A
        };

        // Given a SIS path, identify the EPOC version
        std::optional<sis_type> identify_sis_type(const std::string &path) {
            common::ro_std_file_stream stream(path, true);

            if (!stream.valid()) {
                return std::nullopt;
            }

            std::uint32_t uid1 = 0;
            std::uint32_t uid2 = 0;

            if (stream.read(&uid1, 4) != 4) {
                return std::nullopt;
            }

            if (stream.read(&uid2, 4) != 4) {
                return std::nullopt;
            }

            if (uid1 == static_cast<std::uint32_t>(sis_field_type::SISController)) {
                return sis_type_new_stub;
            }

            if (uid1 == uid1_cst) {
                return sis_type_new;
            }

            if ((uid2 == static_cast<std::uint32_t>(epoc_sis_type::epocu6)) || (uid2 == static_cast<std::uint32_t>(epoc_sis_type::epoc6))) {
                return sis_type_old;
            }

            return std::nullopt;
        }

        sis_contents parse_sis(const std::string &path, const bool is_stub) {
            sis_parser parser(path);

            if (!is_stub)
                parser.parse_header();

            sis_contents cs;

            if (is_stub) {
                cs.controller = parser.parse_controller();
            } else {
                cs = parser.parse_contents();
            }

            return cs;
        }
    }
}
