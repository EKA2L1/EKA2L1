/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <chrono>
#include <common/time.h>

namespace eka2l1::common {
    std::uint64_t get_current_time_in_microseconds_since_1ad() {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
            + ad_epoc_dist_microsecs;
    }

    std::uint64_t convert_microsecs_epoch_to_1ad(const std::uint64_t micsecs) {
        return micsecs * microsecs_per_sec + ad_epoc_dist_microsecs;
    }
    
    std::uint64_t convert_microsecs_win32_1601_epoch_to_1ad(const std::uint64_t micsecs) {
        return micsecs / 10 + ad_win32_epoch_dist_microsecs;
    }
}
