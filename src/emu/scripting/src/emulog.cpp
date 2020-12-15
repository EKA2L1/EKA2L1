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

#include <common/log.h>
#include <scripting/emulog.h>

namespace py = pybind11;

namespace eka2l1::scripting {
    void emulog(const std::string &format, py::args vargs) {
        std::string format_res = format;

        // Since we know all the variable type, it's best that we process all of it
        // This will be O(n^2), where m is number of parantheses pair
        size_t ppos = format_res.find("{}");
        size_t arg_counter = 0;

        while (ppos != std::string::npos) {
            if (arg_counter >= vargs.size()) {
                break;
            }

            const auto &arg = vargs[arg_counter];
            py::str replacement(arg);

            if (!replacement.is_none()) {
                const std::string replacement_std = replacement.cast<std::string>();
                format_res.replace(format_res.begin() + ppos, format_res.begin() + ppos + 2, replacement_std.data());
                ppos = format_res.find("{}");
            }

            arg_counter++;
        }

        LOG_INFO(SCRIPTING, "{}", format_res);
    }
}