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

#include <loader/sis_common.h>
#include <loader/sis_fields.h>

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
        std::optional<epocver> get_epoc_ver(std::string path) {
            FILE *f = fopen(path.c_str(), "rb");

            if (!f) {
                return std::nullopt;
            }

            std::uint32_t uid1 = 0;
            std::uint32_t uid2 = 0;

            if (fread(&uid1, 1, 4, f) != 4) {
                return std::nullopt;
            }

            if (fread(&uid2, 1, 4, f) != 4) {
                return std::nullopt;
            }

            if (uid1 == uid1_cst) {
                return epocver::epoc94;
            }

            if (uid2 == static_cast<std::uint32_t>(epoc_sis_type::epocu6)) {
                return epocver::epocu6;
            }

            return epocver::epoc6;
        }

        sis_contents parse_sis(std::string path) {
            sis_parser parser(path);

            sis_header header = parser.parse_header();
            sis_contents cs = parser.parse_contents();

            return cs;
        }
    }
}
