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

#pragma once

#include <cstdint>

namespace eka2l1::common {
    enum : uint64_t {
        microsecs_per_sec = 1000000,
        ad_epoc_dist_microsecs = 62167132800 * microsecs_per_sec,
        ad_win32_epoch_dist_microsecs = (1601 * 365 * 24 * 60) * microsecs_per_sec
    };

    /**
     * @brief Get total seconds esclaped in microseconds since 1/1/1970.
     * @returns A 64-bit number indicates the total microseconds.
     */
    std::uint64_t get_current_time_in_microseconds_since_epoch();

    /**
     * @brief Get total seconds esclaped in microseconds since 1/1/1AD
     * @returns A 64-bit number indicates the total microseconds.
     */
    std::uint64_t get_current_time_in_microseconds_since_1ad();
    std::uint64_t convert_microsecs_epoch_to_1ad(const std::uint64_t nsecs);
    std::uint64_t convert_microsecs_win32_1601_epoch_to_1ad(const std::uint64_t nsecs);

    /**
     * @brief Get host's UTC offset.
     */
    int get_current_utc_offset();
}
