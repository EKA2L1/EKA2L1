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

#include <epoc/utils/des.h>
#include <cstdint>

namespace eka2l1::epoc {
    enum etel_opcode {
        etel_open_from_session = 0,
        etel_open_from_subsession = 1,
        etel_close = 4,
        etel_enumerate_phones = 54,
        etel_get_tsy_name = 55,
        etel_load_phone_module = 57,
        etel_phone_info_by_index = 59
    };

    enum etel_network_type: std::uint32_t {
        etel_network_type_wired_analog = 0,
        etel_network_type_wired_digital = 1,
        etel_network_type_mobile_analog = 2,
        etel_network_type_mobile_digital = 3,
        etel_network_type_unk = 4
    };

    struct etel_phone_info {
        etel_network_type network_;
        epoc::name phone_name_;
        std::uint32_t line_count_;
        std::uint32_t exts_;
    };
}