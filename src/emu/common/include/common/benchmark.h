/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <common/log.h>
#include <chrono>

namespace eka2l1::common {
    struct benchmarker {
        std::chrono::time_point<std::chrono::steady_clock> start;
        const char *bench_func;

        benchmarker(const char *func)
            : bench_func(func) {
            start = std::chrono::steady_clock::now();
        };

        ~benchmarker() {        
            auto end = std::chrono::steady_clock::now();
            LOG_TRACE("Function {} runned in {} ms ({} s)", bench_func, std::chrono::duration_cast
                <std::chrono::milliseconds>(end-start).count(), std::chrono::duration_cast
                <std::chrono::seconds>(end-start).count());
        }
    };
}
