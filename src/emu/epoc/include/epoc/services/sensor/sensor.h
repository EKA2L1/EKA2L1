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

#include <epoc/services/framework.h>
#include <epoc/services/server.h>
#include <epoc/utils/des.h>

namespace eka2l1 {

    enum sensor_opcode {
        sensor_req_query_channels,
        sensor_req_open_channel,
        sensor_req_close_channel,
        sensor_req_start_listening,
        sensor_req_stop_listening,
        sensor_req_async_channel_data,
        sensor_req_get_property,
        sensor_req_async_property_data,
        sensor_req_stop_property_listening,
        sensor_req_get_all_properties
    };

    constexpr std::uint32_t LOCATION_LENGTH = 16;
    constexpr std::uint32_t VENDOR_ID_LENGTH = 16;

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

    enum channel_types {
        accelerometer_xyz_axis_data = 0x1020507E
    };

    class sensor_server : public service::typical_server {
    public:
        explicit sensor_server(eka2l1::system *sys);

        void connect(service::ipc_context &context) override;
    };

    struct sensor_client_session : public service::typical_session {
        explicit sensor_client_session(service::typical_server *serv, const std::uint32_t ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
    };
}
