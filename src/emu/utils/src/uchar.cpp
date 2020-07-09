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
#include <cctype>

namespace eka2l1::epoc {
    // TODO (bent): Probably not really accurate yet.
    std::uint32_t get_uchar_category(const uchar c, std::locale &ln) {
        wchar_t the_wc = static_cast<wchar_t>(c);

        if (std::iscntrl(the_wc, ln)) {
            return uchar_category::UCHAR_CATEGORY_OTHER_CONTROL;
        }

        if (std::ispunct(the_wc, ln)) {
            // TODO (bent): Expanse this into specific category
            return uchar_category::UCHAR_CATEGORY_PUNCTUATION_GROUP;
        }

        if (std::isalpha(the_wc, ln)) {
            if (std::islower(the_wc, ln)) {
                return uchar_category::UCHAR_CATEGORY_LETTER_LOWERCASE;
            } else {
                return uchar_category::UCHAR_CATEGORY_LETTER_UPPERCASE;
            }
        }

        if (std::isspace(the_wc, ln)) {
            return uchar_category::UCHAR_CATEGORY_SEPARATOR_SPACE;
        }

        if (std::isdigit(the_wc, ln)) {
            return uchar_category::UCHAR_CATEGORY_NUMBER_DECIMAL_DIGIT;
        }

        return uchar_category::UCHAR_CATEGORY_OTHER_NOT_ASSIGNED;
    }

    const uchar uppercase_uchar(const uchar c, std::locale &ln) {
        return std::toupper(static_cast<wchar_t>(c), ln);
    }
    
    const uchar lowercase_uchar(const uchar c, std::locale &ln) {
        return std::tolower(static_cast<wchar_t>(c), ln);
    }
    
    const uchar fold_uchar(const uchar c, std::locale &ln) {
        // TODO: Proper implementation. For now just uppercase so it does not yell
        return uppercase_uchar(c, ln);
    }
}