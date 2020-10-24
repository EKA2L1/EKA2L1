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

#include <system/epoc.h>
#include <services/sensor/sensor.h>

#include <utils/err.h>

namespace eka2l1 {
    sensor_server::sensor_server(eka2l1::system *sys)
        : service::typical_server(sys, "!SensorServer") {
        if (!initialized) {
            init();
            initialized = true;
        }
    }

    void sensor_server::connect(service::ipc_context &context) {
        create_session<sensor_client_session>(&context);
        context.complete(epoc::error_none);
    }

    void sensor_server::init() {
        for (size_t i = 0; i < ACCEL_PROPERTY_COUNT; i++) {
            sensor_property res_property;
            res_property.property_id = property_ids::axis_active;
            res_property.property_type = property_types::int_property;
            res_property.array_index = property_array_index::single_property;
            res_property.item_index = static_cast<std::int32_t>(i + 1);

            res_property.real_value = 0;
            res_property.real_value_max = 1;
            res_property.real_value_min = -1;
            accel_properties.push_back(res_property);
        }

        channel_info info;
        info.channel_id = 0;
        info.channel_type = channel_types::accelerometer_xyz_axis_data;
        info.channel_data_type_id = channel_types::accelerometer_xyz_axis_data;
        channel_infos.push_back(info);
    }

    sensor_client_session::sensor_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void sensor_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case sensor_query_channels: {
            query_channels(ctx);
            break;
        }

        case sensor_open_channel: {
            open_channel(ctx);
            break;
        }

        case sensor_close_channel: {
            close_channel(ctx);
            break;
        }

        case sensor_start_listening: {
            start_listening(ctx);
            break;
        }

        case sensor_stop_listening: {
            stop_listening(ctx);
            break;
        }

        case sensor_get_property: {
            get_property(ctx);
            break;
        }

        case sensor_get_all_properties: {
            get_all_properties(ctx);
            break;
        }

        default: {
            LOG_ERROR("Unimplemented opcode for Sensor server 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }

    void sensor_client_session::query_channels(eka2l1::service::ipc_context *ctx) {
        std::optional<channel_info> search_cond = ctx->get_argument_data_from_descriptor<channel_info>(0);
        channel_info *list = (channel_info *)ctx->get_descriptor_argument_ptr(1);
        std::uint32_t *channel_info_count_ptr = reinterpret_cast<std::uint32_t *>(ctx->get_descriptor_argument_ptr(2));

        std::uint32_t channel_info_count = 0;
        if (search_cond->channel_type == 0 || search_cond->channel_type == channel_types::accelerometer_xyz_axis_data) {
            channel_info_count = 1;
            list[0] = server<sensor_server>()->channel_infos.at(0);
        }

        ctx->write_data_to_descriptor_argument(2, channel_info_count);
        ctx->set_descriptor_argument_length(1, channel_info_count * sizeof(channel_info));
        ctx->complete(epoc::error_none);
    }

    void sensor_client_session::open_channel(eka2l1::service::ipc_context *ctx) {
        std::uint32_t channel_id = *(ctx->get_argument_value<std::uint32_t>(0));
        std::uint32_t *max_buffer_count = reinterpret_cast<std::uint32_t *>(ctx->get_descriptor_argument_ptr(1));

        ctx->complete(epoc::error_none);
    }

    void sensor_client_session::close_channel(eka2l1::service::ipc_context *ctx) {
        std::uint32_t channel_id = *(ctx->get_argument_value<std::uint32_t>(0));

        ctx->complete(epoc::error_none);
    }

    void sensor_client_session::start_listening(eka2l1::service::ipc_context *ctx) {
        std::uint32_t channel_id = *(ctx->get_argument_value<std::uint32_t>(0));
        std::optional<listening_parameters> params = ctx->get_argument_data_from_descriptor<listening_parameters>(1);

        ctx->complete(epoc::error_none);
    }

    void sensor_client_session::stop_listening(eka2l1::service::ipc_context *ctx) {
        std::uint32_t channel_id = *(ctx->get_argument_value<std::uint32_t>(0));

        ctx->complete(epoc::error_none);
    }

    void sensor_client_session::get_property(eka2l1::service::ipc_context *ctx) {
        std::uint32_t channel_id = *(ctx->get_argument_value<std::uint32_t>(0));
        std::optional<sensor_property> res_property = ctx->get_argument_data_from_descriptor<sensor_property>(1);

        bool found = false;
        for (size_t i = 0; i < ACCEL_PROPERTY_COUNT; i++) {
            sensor_property property = server<sensor_server>()->accel_properties.at(i);
            if (property.property_id == res_property->property_id && property.item_index == res_property->item_index) {
                res_property = property;
                found = true;
                break;
            }
        }

        if (!found) {
            ctx->complete(epoc::error_argument);
            return;
        }

        ctx->write_data_to_descriptor_argument(1, res_property);
        ctx->complete(epoc::error_none);
    }

    void sensor_client_session::get_all_properties(eka2l1::service::ipc_context *ctx) {
        std::uint32_t channel_id = *(ctx->get_argument_value<std::uint32_t>(0));
        sensor_property *list = (sensor_property *)ctx->get_descriptor_argument_ptr(1);
        std::uint32_t *property_count_ptr = reinterpret_cast<std::uint32_t *>(ctx->get_descriptor_argument_ptr(2));

        std::uint32_t property_count = ACCEL_PROPERTY_COUNT;
        for (size_t i = 0; i < property_count; i++) {
            sensor_property res_property = server<sensor_server>()->accel_properties.at(i);
            list[i] = res_property;
        }
        ctx->write_data_to_descriptor_argument(2, property_count);
        ctx->set_descriptor_argument_length(1, property_count * sizeof(sensor_property));
        ctx->complete(epoc::error_none);
    }
}
