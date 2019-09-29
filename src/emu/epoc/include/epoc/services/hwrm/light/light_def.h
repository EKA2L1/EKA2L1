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

#pragma once

#include <cstdint>

namespace eka2l1::epoc::hwrm::light {
    /**
     * \brief The light target.
     * 
     * Each light is belong to a specific device, which we call target. With the target specified,
     * you can control the light of the device as you wish.
     */
    enum target {
        none = 0,
        primary_display = (1 << 0),
        primary_keyboard = (1 << 1),
        secondary_display = (1 << 2),
        secondary_keyboard = (1 << 3),
        custom_target_1 = (1 << 4),
        custom_target_2 = (1 << 5),
        custom_target_3 = (1 << 6),
        custom_target_4 = (1 << 7),
        custom_target_5 = (1 << 8),
        custom_target_6 = (1 << 9),
        custom_target_7 = (1 << 10),
        custom_target_8 = (1 << 11),
        custom_target_9 = (1 << 12),
        custom_target_10 = (1 << 13),
        custom_target_11 = (1 << 14),
        custom_target_12 = (1 << 15),
        custom_target_13 = (1 << 16),
        custom_target_14 = (1 << 17),
        custom_target_15 = (1 << 18),
        custom_target_16 = (1 << 19),
        custom_target_17 = (1 << 20),
        custom_target_18 = (1 << 21),
        custom_target_19 = (1 << 22),
        custom_target_20 = (1 << 23),
        custom_target_21 = (1 << 24),
        custom_target_22 = (1 << 25),
        custom_target_23 = (1 << 26),
        custom_target_24 = (1 << 27),
        custom_target_25 = (1 << 28),
        custom_target_26 = (1 << 29),
        custom_target_27 = (1 << 30)
    };

    /**
     * \brief The status of a light.
     */
    enum status {
        light_status_unk = 0,       ///< Unknown status.
        light_status_on = 1,        ///< The light is on.
        light_status_off = 2,       ///< The light is off
        light_status_blink = 3      ///< The light is blinking.
    };

    // The phone is not a power supply you can't just have a festival on it.
    static constexpr std::uint32_t MAXIMUM_LIGHT = 31;      ///< Maximum light a device can have.
}