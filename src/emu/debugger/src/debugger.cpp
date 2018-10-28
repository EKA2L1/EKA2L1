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

#include <algorithm>
#include <debugger/debugger.h>

namespace eka2l1 {
    void debugger_base::set_breakpoint(const std::uint32_t bkpt_addr, const bool hit) {
        const std::lock_guard<std::mutex> guard(lock);

        auto bkpt = std::find_if(breakpoints.begin(), breakpoints.end(), [bkpt_addr](debug_breakpoint &b) {
            return b.addr == bkpt_addr;
        });

        if (bkpt == breakpoints.end()) {
            breakpoints.push_back(debug_breakpoint{ bkpt_addr, hit });
        }

        std::sort_heap(breakpoints.begin(), breakpoints.end(), [](const debug_breakpoint &lhs, const debug_breakpoint &rhs) {
            return lhs.addr < rhs.addr;
        });
    }

    void debugger_base::unset_breakpoint(const std::uint32_t bkpt_addr) {
        const std::lock_guard<std::mutex> guard(lock);

        auto bkpt = std::find_if(breakpoints.begin(), breakpoints.end(), [bkpt_addr](debug_breakpoint &b) {
            return b.addr == bkpt_addr;
        });

        if (bkpt != breakpoints.end()) {
            breakpoints.erase(bkpt);
        }

        std::sort_heap(breakpoints.begin(), breakpoints.end(), [](const debug_breakpoint &lhs, const debug_breakpoint &rhs) {
            return lhs.addr < rhs.addr;
        });
    }

     std::optional<debug_breakpoint> debugger_base::get_nearest_breakpoint(const std::uint32_t bkpt) {
        for (std::size_t i = 0; i < breakpoints.size(); i++) {
            if (breakpoints[i].addr > bkpt) {
                if (i == 0) {
                    break;
                }

                return breakpoints[i - 1];
            }
        }

        return std::optional<debug_breakpoint>{};
    }
}