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

#include <common/types.h>
#include <iostream>
#include <sstream>

namespace eka2l1 {
    namespace common {
		/*! \brief Convert an UCS2 string to an UTF8 string. */
        std::string ucs2_to_utf8(std::u16string str);

		/*! \brief Convert something to string. */
        template <class T>
        std::string to_string(T t, std::ios_base &(*f)(std::ios_base &)) {
            std::ostringstream oss;
            oss << f << t;
            return oss.str();
        }

		/*! \brief Convert something to string. */
        template <class T>
        std::string to_string(T t) {
            std::ostringstream oss;
            oss << t;
            return oss.str();
        }
    }
}

