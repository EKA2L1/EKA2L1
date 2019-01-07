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
#include <common/cvt.h>

#include <codecvt>
#include <locale>

namespace eka2l1 {
    namespace common {
#ifdef _MSC_VER
        using char_ucs2 = uint16_t;
#else
        using char_ucs2 = char16_t;
#endif

        // VS2017 bug: https://stackoverflow.com/questions/32055357/visual-studio-c-2015-stdcodecvt-with-char16-t-or-char32-t
        std::string ucs2_to_utf8(std::u16string str) {
            std::wstring_convert<std::codecvt_utf8_utf16<char_ucs2>, char_ucs2> convert;
            auto p = reinterpret_cast<const char_ucs2 *>(str.data());

            return convert.to_bytes(p, p + str.size());
        }

        std::u16string utf8_to_ucs2(const std::string &str) {
            std::wstring_convert<std::codecvt_utf8_utf16<char_ucs2>, char_ucs2> converter;
            auto wstr = converter.from_bytes(str);

            return std::u16string(wstr.begin(), wstr.end());
        }
    }
}
