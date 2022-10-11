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
#include <utils/des.h>

namespace eka2l1::epoc::acc {
    static constexpr std::uint32_t GENERIC_ID_ARRAY_COUNT = 10;
    static constexpr std::uint32_t MODEL_ID_MAX_LENGTH = 32;

    // There are massive changes to the enum in further Symbian future. But s60v3 keeps it simple
    enum opcode_s60v3 {
        opcode_s60v3_create_accessory_connection_subsession = 7,
        opcode_s60v3_get_accessory_connection_status = 9,
        opcode_s60v3_notify_new_accessory_connected = 11,
        opcode_s60v3_cancel_notify_new_accessory_connected = 12
    };

#pragma pack(push, 1)
    struct generic_id_header {
        std::uint32_t acc_device_type_;
        std::uint32_t physical_connection_;
        std::uint32_t application_protocol_;
        std::uint64_t caps_subblocks_;
        epoc::buf_static<char16_t, MODEL_ID_MAX_LENGTH> hw_model_id_;
        std::uint64_t hw_device_id_;
        std::int32_t dbid_;
    };

    struct generic_id {
        generic_id_header header_;
    };

    struct generic_id_array {
        generic_id ids_[GENERIC_ID_ARRAY_COUNT];

        explicit generic_id_array();
    };
#pragma pack(pop)
}