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
#pragma once

#include <cstdint>
#include <string>

namespace eka2l1 {
    namespace common {
        // https://stackoverflow.com/questions/7968674/unexpected-collision-with-stdhash
        uint32_t hash(std::string const &s) {
            uint32_t h = 5381;

            for (auto c : s)
                h = ((h << 5) + h) + c; /* hash * 33 + c */

            return h;
        }

        std::string normalize_for_hash(std::string org) {
            auto remove = [](std::string &inp, std::string to_remove) {
                size_t pos = 0;

                do {
                    pos = inp.find(to_remove, pos);

                    if (pos == std::string::npos) {
                        break;
                    } else {
                        inp.erase(pos, to_remove.length());
                    }
                } while (true);
            };

            for (auto &c : org) {
                c = tolower(c);
            }

            remove(org, " ");
            // Remove class in arg

            std::size_t beg = org.find("(");
            std::size_t end = org.find(")");

            std::string sub = org.substr(beg, end);

            remove(sub, "class");
            remove(sub, "const");
            remove(sub, "struct");

            auto res = org.substr(0, beg) + sub + org.substr(end + 1);

            return res;
        }
    }
}

