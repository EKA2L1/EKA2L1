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

#include <common/platform.h>

#include <codecvt>
#include <locale>

#if EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#endif

namespace eka2l1 {
    namespace common {
#ifdef _MSC_VER
        using char_ucs2 = uint16_t;
#else
        using char_ucs2 = char16_t;
#endif

        std::wstring_convert<std::codecvt_utf8_utf16<char_ucs2>, char_ucs2> ucs2_to_utf8_converter;
        std::wstring_convert<std::codecvt_utf8_utf16<char_ucs2>, char_ucs2> utf8_to_ucs2_converter;
        std::wstring_convert<std::codecvt_utf16<wchar_t, 0x10ffff, std::little_endian>, wchar_t> ucs2_to_wstr_converter;
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8_to_wstr_converter;
        std::wstring_convert<std::codecvt_utf8<wchar_t>> wstr_to_utf8_converter;

        // VS2017 bug: https://stackoverflow.com/questions/32055357/visual-studio-c-2015-stdcodecvt-with-char16-t-or-char32-t
        std::string ucs2_to_utf8(const std::u16string &str) {
            if (str.empty()) {
                return "";
            }

#if EKA2L1_PLATFORM(WIN32)
            // Wide-char backward compatible to UCS2
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<LPCWCH>(str.data()), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
            std::string result(size_needed, 0);

            WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<LPCWCH>(str.data()), static_cast<int>(str.size()), result.data(), size_needed, NULL, NULL);
            return result;
#else
            auto p = reinterpret_cast<const char_ucs2 *>(str.data());
            return ucs2_to_utf8_converter.to_bytes(p, p + str.size());
#endif
        }

        std::u16string utf8_to_ucs2(const std::string &str) {
            if (str.empty()) {
                return u"";
            }

#if EKA2L1_PLATFORM(WIN32)
            // Wishful thinking that Symbian can understand the result in all cases
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0);
            std::u16string ustr(size_needed, 0);

            MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), reinterpret_cast<LPWSTR>(ustr.data()), size_needed);
            return ustr;
#else
            auto wstr = utf8_to_ucs2_converter.from_bytes(str);

            std::u16string new_string(wstr.begin(), wstr.end());
            return new_string;
#endif
        }

        std::wstring ucs2_to_wstr(const std::u16string &str) {
#if EKA2L1_PLATFORM(WIN32)
            return std::wstring(reinterpret_cast<const wchar_t *>(str.data()), str.size());
#else
            auto wstr = ucs2_to_wstr_converter.from_bytes(reinterpret_cast<const char *>(&str[0]),
                reinterpret_cast<const char *>(&str[0] + str.size()));

            return wstr;
#endif
        }

        std::wstring utf8_to_wstr(const std::string &str) {
#if EKA2L1_PLATFORM(WIN32)
            // Wishful thinking that Symbian can understand the result in all cases
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0);
            std::wstring wstr(size_needed, 0);

            MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wstr.data(), size_needed);
            return wstr;
#else
            auto wstr = utf8_to_wstr_converter.from_bytes(reinterpret_cast<const char *>(&str[0]),
                reinterpret_cast<const char *>(&str[0] + str.size()));

            return wstr;
#endif
        }

        std::string wstr_to_utf8(const std::wstring &str) {
#if EKA2L1_PLATFORM(WIN32)
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
            std::string result(size_needed, 0);

            WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), size_needed, NULL, NULL);
            return result;
#else
            return wstr_to_utf8_converter.to_bytes(str.data(), str.data() + str.size());
#endif
        }
    }
}
