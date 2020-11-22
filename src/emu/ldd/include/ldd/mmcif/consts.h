/*
 * Copyright (c) 2020 EKA2L1 Team.
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

namespace eka2l1::ldd {
    static const char *MMCIF_FACTORY_NAME = "MmcIf";

    enum mmc_media_type {
        mmc_media_type_rom,
        mmc_media_type_flash,
        mmc_media_type_io,
        mmc_media_type_other,
        mmc_media_type_not_supported
    };

    enum mmc_control_opcode {
        mmc_control_reset = 0,
        mmc_control_power_down = 1,
        mmc_control_register_event = 2,
        mmc_control_select_card = 3,
        mmc_control_stack_info = 4,
        mmc_control_card_info = 5
    };
}