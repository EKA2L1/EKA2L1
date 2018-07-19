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
#include <core/abi/eabi.h>
#include <cxxabi.h>
#include <memory>

namespace eka2l1 {
    // Global abi
    namespace eabi {
        std::string demangle(std::string target) {
            char *out_buf = reinterpret_cast<char *>(std::malloc(target.length() * 3));
            int status = 0;
            char *res = abi::__cxa_demangle(target.data(), out_buf, nullptr, &status);

            if (status == 0) {
                return res;
            }

            return target;
        }

        std::string mangle(std::string target) {
            return target;
        }
    }
}
