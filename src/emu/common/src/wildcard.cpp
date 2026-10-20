/*
 * Copyright (c) 2019 EKA2L1 Team.
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
#include <common/wildcard.h>
#include <common/cvt.h>
#include <re2/re2.h>

#include <string_view>
#include <type_traits>

namespace eka2l1::common {
    template <typename T>
    static std::basic_string<T> wildcard_to_regex_string_impl(const std::basic_string<T> &wildcard, bool case_sensitive) {
        static constexpr std::string_view REGEX_SPECIALS = "\\.^$|()[]{}+";
        std::basic_string<T> regexstr;

        if (!case_sensitive) {
            regexstr += {T('('), T('?'), T('i'), T(')')};
        }

        for (const T c : wildcard) {
            if (c == T('*')) {
                regexstr += {T('.'), T('*')};
            } else if (c == T('?')) {
                regexstr += T('.');
            } else {
                if ((static_cast<std::make_unsigned_t<T>>(c) < 0x80) && (REGEX_SPECIALS.find(static_cast<char>(c)) != std::string_view::npos)) {
                    regexstr += T('\\');
                }

                regexstr += c;
            }
        }

        return regexstr;
    }

    template <>
    std::basic_string<char> wildcard_to_regex_string(std::basic_string<char> regexstr, bool case_sensitive) {
        return wildcard_to_regex_string_impl(regexstr, case_sensitive);
    }

    template <>
    std::basic_string<wchar_t> wildcard_to_regex_string(std::basic_string<wchar_t> regexstr, bool case_sensitive) {
        return wildcard_to_regex_string_impl(regexstr, case_sensitive);
    }

    bool full_wildcard_match_impl(const std::string &reference, const std::string &match_pattern,
        const bool is_fold) {
        return RE2::FullMatch(reference, wildcard_to_regex_string(match_pattern, !is_fold));
    }

    template <> bool full_wildcard_match<char>(const std::string &reference, const std::string &match_pattern,
        const bool is_fold) {
        return full_wildcard_match_impl(reference, match_pattern, is_fold);
    }

    template <> bool full_wildcard_match<wchar_t>(const std::wstring &reference, const std::wstring &match_pattern,
        const bool is_fold) {
        return full_wildcard_match_impl(common::wstr_to_utf8(reference), common::wstr_to_utf8(match_pattern), is_fold);
    }

    std::size_t wildcard_match_impl(const std::string &reference, const std::string &match_pattern,
        const bool is_fold) {
        std::string rgs = wildcard_to_regex_string(match_pattern, !is_fold);

        re2::StringPiece result;
        if (!RE2::PartialMatch(reference, rgs, &result)) {
            if (RE2::FullMatch(reference, rgs)) {
                return 0;
            }

            return std::string::npos;
        }

        return result.data() - reference.data();
    }

    template <> std::size_t wildcard_match<char>(const std::string &reference, const std::string &match_pattern,
        const bool is_fold) {
        return wildcard_match_impl(reference, match_pattern, is_fold);
    }

    template <> std::size_t wildcard_match<wchar_t>(const std::wstring &reference, const std::wstring &match_pattern,
        const bool is_fold) {
        return wildcard_match_impl(common::wstr_to_utf8(reference), common::wstr_to_utf8(match_pattern), is_fold);
    }
}
