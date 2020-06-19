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

#include <utils/uchar.h>

namespace eka2l1::epoc {
    // TODO (bent): Probably not really accurate yet.
    std::uint32_t get_uchar_category(const uchar c, std::locale &ln) {
        if (std::iscntrl(c, ln)) {
            return uchar_category::UCHAR_CATEGORY_OTHER_CONTROL;
        }

        if (std::ispunct(c, ln)) {
            // TODO (bent): Expanse this into specific category
            return uchar_category::UCHAR_CATEGORY_PUNCTUATION_GROUP;
        }

        if (std::isalpha(c, ln)) {
            if (std::islower(c, ln)) {
                return uchar_category::UCHAR_CATEGORY_LETTER_LOWERCASE;
            } else {
                return uchar_category::UCHAR_CATEGORY_LETTER_UPPERCASE;
            }
        }

        if (std::isspace(c, ln)) {
            return uchar_category::UCHAR_CATEGORY_SEPARATOR_SPACE;
        }

        if (std::isdigit(c, ln)) {
            return uchar_category::UCHAR_CATEGORY_NUMBER_DECIMAL_DIGIT;
        }

        return uchar_category::UCHAR_CATEGORY_OTHER_NOT_ASSIGNED;
    }

    const uchar uppercase_uchar(const uchar c, std::locale &ln) {
        return std::toupper(c, ln);
    }
    
    const uchar lowercase_uchar(const uchar c, std::locale &ln) {
        return std::tolower(c, ln);
    }
}