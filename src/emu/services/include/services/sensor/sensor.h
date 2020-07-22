/*
 * Copyright (c) 2020 EKA2L1 Team
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

#pragma once

#include <services/framework.h>
#include <kernel/server.h>
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

    constexpr std::uint32_t ACCEL_PROPERTY_COUNT = 3;

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

    struct listening_parameters {
        std::uint32_t desired_buffering_count;
        std::uint32_t maximum_buffering_count;
        std::uint32_t buffering_period;
    };

    struct sensor_property {
        std::uint32_t property_id;
        std::int32_t item_index;
        std::int16_t array_index;
        std::uint64_t real_value;
        epoc::buf_static<char, PROPERTY_TEXT_BUFFER_LENGTH> buf_value;
        std::uint32_t flags;
        std::uint64_t real_value_max;
        std::uint64_t real_value_min;
        std::uint32_t property_type;
        epoc::security_info sec_info;
        std::uint32_t reserved;
        // TODO: This struct shouldn't contain these 24 bytes
        std::uint64_t reserved2;
        std::uint64_t reserved3;
        std::uint64_t reserved4;
    };

    enum property_types {
        uninitialized_property,
        int_property,
        real_property,
        buffer_property
    };

    enum property_array_index {
        single_property = -1,
        array_property = -2
    };

    enum property_ids {
        data_rate = 0x2,
        axis_active = 0x1001
    };

    enum channel_types {
        accelerometer_xyz_axis_data = 0x1020507E
    };

    class sensor_server : public service::typical_server {
        bool initialized{ false };

        void init();

    public:
        std::vector<channel_info> channel_infos;
        std::vector<sensor_property> accel_properties;

        explicit sensor_server(eka2l1::system *sys);

        void connect(service::ipc_context &context) override;
    };

    struct sensor_client_session : public service::typical_session {
        explicit sensor_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void query_channels(eka2l1::service::ipc_context *ctx);
        void open_channel(eka2l1::service::ipc_context *ctx);
        void close_channel(eka2l1::service::ipc_context *ctx);
        void start_listening(eka2l1::service::ipc_context *ctx);
        void stop_listening(eka2l1::service::ipc_context *ctx);
        void get_property(eka2l1::service::ipc_context *ctx);
        void get_all_properties(eka2l1::service::ipc_context *ctx);
    };
}
