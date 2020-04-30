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

namespace eka2l1 {
    enum hwrm_fundamental_op {
        hwrm_fundamental_op_create_vibration_service = 0,
        hwrm_fundamental_op_create_light_service = 1,
        hwrm_fundamental_op_create_power_service = 2,
        hwrm_fundamental_op_create_fm_tx_service = 3
    };

    enum hwrm_light_op {
        hwrm_light_op_on = 1000,
        hwrm_light_op_off = 1001,
        hwrm_light_op_blink = 1002,
        hwrm_light_op_cleanup_lights = 1003,
        hwrm_light_op_reserve_lights = 1004,
        hwrm_light_op_release_lights = 1005,
        hwrm_light_op_supported_targets = 1006,
        hwrm_light_op_set_light_color = 1007
    };

    enum hwrm_vibrate_op {
        hwrm_vibrate_start_with_default_intensity = 2000,
        hwrm_vibrate_cleanup = 2006
    };
}