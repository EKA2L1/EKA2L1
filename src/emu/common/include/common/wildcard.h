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

#pragma once

#include <common/algorithm.h>
#include <regex>

namespace eka2l1::common {
    /**
     * \brief Convert a wildcard string to regex 
     */
    template <typename T>
    std::basic_string<T> wildcard_to_regex_string(std::basic_string<T> regexstr);

    template <typename T>
    std::size_t match_wildcard_in_string(const std::basic_string<T> &reference, const std::basic_string<T> &match_pattern,
        const bool is_fold);
}
