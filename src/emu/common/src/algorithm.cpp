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
#include <common/algorithm.h>

namespace eka2l1 {
    namespace common {
        size_t find_nth(std::string targ, std::string str, size_t idx, size_t pos) {
            size_t found_pos = targ.find(str, pos);

            if (1 == idx || found_pos == std::string::npos) {
                return found_pos;
            }

            return find_nth(targ, str, idx - 1, found_pos + 1);
        }

        void remove(std::string &inp, std::string to_remove) {
            size_t pos = 0;

            do {
                pos = inp.find(to_remove, pos);

                if (pos == std::string::npos) {
                    break;
                } else {
                    inp.erase(pos, to_remove.length());
                }
            } while (true);
        }
    }
}
