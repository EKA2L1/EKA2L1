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
#include <utils/sec.h>

namespace eka2l1 {
    enum sensor_opcode {
        sensor_query_channels,
        sensor_open_channel,
        sensor_close_channel,
        sensor_start_listening,
        sensor_stop_listening,
        sensor_async_channel_data,
        sensor_get_property,
        sensor_async_property_data,
        sensor_stop_property_listening,
        sensor_get_all_properties
    };

    constexpr std::uint32_t LOCATION_LENGTH = 16;
    constexpr std::uint32_t VENDOR_ID_LENGTH = 16;
    constexpr std::uint32_t PROPERTY_TEXT_BUFFER_LENGTH = 20;

    #pragma pack(push, 1)
    struct channel_info {
        std::uint32_t channel_id;
        std::uint32_t context_type;
        std::uint32_t quantity;
        std::uint32_t channel_type;
        epoc::buf_static<char, LOCATION_LENGTH> location;
        epoc::buf_static<char, VENDOR_ID_LENGTH> vendor_id;
        std::uint32_t data_item_size;
        std::uint32_t reserved3;
        std::uint32_t channel_data_type_id;
        std::uint32_t reserved;
        std::uint32_t reserved2;
    };
    #pragma pack(pop)

    struct listening_parameters {
        std::uint32_t desired_buffering_count;
        std::uint32_t maximum_buffering_count;
        std::uint32_t buffering_period;
    };

    struct data_count_ret_val {
        std::uint32_t item_count_;
        std::uint32_t lost_count_;
    };

    static_assert(sizeof(data_count_ret_val) == 8);

    #pragma pack(push, 1)
    struct sensor_property {
        std::uint32_t property_id;
        std::int32_t item_index;
        std::int16_t array_index;
        std::int16_t padding1;
        std::uint32_t paddding2;

        union {
            std::uint32_t int_value;
            double real_value;
        };

        epoc::buf_static<char, PROPERTY_TEXT_BUFFER_LENGTH> buf_value;
        std::uint32_t flags;

        union {
            std::uint32_t int_value_max;
            double real_value_max;
        };

        union {
            std::uint32_t int_value_min;
            double real_value_min;
        };

        std::uint32_t property_type;
        epoc::security_info sec_info;
        std::uint32_t reserved;
    };

    static_assert(offsetof(sensor_property, property_type) == 72);
    #pragma pack(pop)

    enum property_types {
        uninitialized_property,
        int_property,
        real_property,
        buffer_property
    };
}