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

namespace eka2l1::common {
    template <>
    std::basic_string<char> wildcard_to_regex_string(std::basic_string<char> regexstr) {
        regexstr = replace_all<char>(regexstr, "\\", "\\\\");
        regexstr = replace_all<char>(regexstr, ".", std::string("\\") + ".");
        regexstr = replace_all<char>(regexstr, "?", ".");
        regexstr = replace_all<char>(regexstr, "*", ".*");
        regexstr = replace_all<char>(regexstr, "[", "(");
        regexstr = replace_all<char>(regexstr, "]", ")");

        return regexstr;
    }

    template <>
    std::basic_string<wchar_t> wildcard_to_regex_string(std::basic_string<wchar_t> regexstr) {
        regexstr = replace_all<wchar_t>(regexstr, L"\\", L"\\\\");
        regexstr = replace_all<wchar_t>(regexstr, L".", std::wstring(L"\\") + L".");
        regexstr = replace_all<wchar_t>(regexstr, L"?", L".");
        regexstr = replace_all<wchar_t>(regexstr, L"*", L".*");
        regexstr = replace_all<wchar_t>(regexstr, L"[", L"(");
        regexstr = replace_all<wchar_t>(regexstr, L"]", L")");

        return regexstr;
    }

    template <typename T>
    std::size_t match_wildcard_in_string(const std::basic_string<T> &reference, const std::basic_string<T> &match_pattern,
        const bool is_fold) {  
        std::basic_regex<T> reg(wildcard_to_regex_string(match_pattern), (is_fold ? std::regex_constants::icase :
            std::regex_constants::basic));

        std::match_results<typename std::basic_string<T>::const_iterator> match_result;
        if (std::regex_search(reference, match_result, reg)) {
            return static_cast<std::size_t>(match_result.position());
        }

        return std::basic_string<T>::npos;
    }
    
    template std::size_t match_wildcard_in_string<char>(const std::string &reference, const std::string &match_pattern,
        const bool is_fold);
    template std::size_t match_wildcard_in_string<wchar_t>(const std::wstring &reference, const std::wstring &match_pattern,
        const bool is_fold);
}
