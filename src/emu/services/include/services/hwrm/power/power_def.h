/*
 * Copyright (c) 2021 EKA2L1 Team
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

#pragma once

#include <cstdint>

namespace eka2l1::epoc::hwrm::power {
    static constexpr std::uint32_t STATE_UID = 0x10205041;
    static constexpr std::uint32_t BATTERY_LEVEL_KEY = 1;
    static constexpr std::uint32_t BATTERY_STATUS_KEY = 2;
    static constexpr std::uint32_t CHARGING_STATUS_KEY = 3;

    enum charging_status {
        CHARGING_STATUS_ERROR = -1,
        CHARGING_STATUS_NOT_CONNECTED = 0,
        CHARGING_STATUS_CHARGING = 1,
        CHARGING_STATUS_NOT_CHARGING = 2,
        CHARGING_STATUS_ALMOST_COMPLETE = 3,
        CHARGING_STATUS_COMPLETE = 4,
        CHARGING_STATUS_CONTINUE = 5
    };

    enum battery_status {
        BATTERY_STATUS_UNKOWN = -1,
        BATTERY_STATUS_OK = 0,
        BATTERY_STATUS_LOW = 1,
        BATTERY_STATUS_EMPTY = 2
    };

    enum battery_level {
        BATTERY_LEVEL_UNKOWN = -1,
        BATTERY_LEVEL_MIN = 0,
        BATTERY_LEVEL_MAX = 7
    };
}