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
#include <common/platform.h>

#if EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#endif

#include <cwctype>

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

        int compare_ignore_case(const utf16_str &s1,
            const utf16_str &s2) {
#if EKA2L1_PLATFORM(WIN32)
            // According to the MSDN documentation, the CompareStringEx function
            // is optimized for NORM_IGNORECASE and string lengths specified as -1.

            const int result = CompareStringEx(
                LOCALE_NAME_INVARIANT,
                NORM_IGNORECASE,
                reinterpret_cast<const wchar_t *>(s1.c_str()),
                -1,
                reinterpret_cast<const wchar_t *>(s2.c_str()),
                -1,
                nullptr, // reserved
                nullptr, // reserved
                0 // reserved
            );

            switch (result) {
            case CSTR_EQUAL:
                return 0;

            case CSTR_GREATER_THAN:
                return 1;

            case CSTR_LESS_THAN:
                return -1;

            default:
                return -2;
            }
#else
            if (s1.size() == s2.size()) {
                for (size_t i = 0; i < s1.size(); i++) {
                    const wchar_t t1 = towlower(s1[i]);
                    const wchar_t t2 = towlower(s2[i]);

                    if (t1 > t2) {
                        return 1;
                    } else if (t1 < t2) {
                        return -1;
                    }
                }

                return 0;
            }

            if (s1.size() > s2.size()) {
                return 1;
            }

            return -1;
#endif
        }

        std::string trim_spaces(std::string str) {
            std::string::iterator new_end = std::unique(str.begin(), str.end(), [](char lhs, char rhs) {
                return (lhs == rhs) && (lhs == ' ');
            });

            str.erase(new_end, str.end());
            
            while (str.length() > 0 && str[0] == ' ') {
                str.erase(str.begin());
            }

            while (str.length() > 0 && str.back() == ' ') {
                str.erase(str.length() - 1);
            }

            return str;
        }
    }
}
