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

#include <services/sensor/sensor.h>
#include <system/epoc.h>
#include <utils/err.h>

#include <drivers/sensor/sensor.h>
#include <common/cvt.h>

namespace eka2l1 {
    sensor_channel::sensor_channel(std::unique_ptr<drivers::sensor> &sensor)
        : controller_(std::move(sensor))
        , new_data_callback_handle_(0) {

    }
    
    sensor_channel::~sensor_channel() {
        cancel_data_listening();
    }

    bool sensor_channel::listen_for_data(const listening_parameters &parameters) {
        // TODO!
        return true;
    }

    void sensor_channel::cancel_data_listening() {
        if (!new_data_callback_handle_ && controller_) {
            controller_->cancel_data_listening(new_data_callback_handle_);
        }
    }
    
    sensor_server::sensor_server(eka2l1::system *sys)
        : service::typical_server(sys, "!SensorServer") {
    }

    void sensor_server::connect(service::ipc_context &context) {
        create_session<sensor_client_session>(&context);
        context.complete(epoc::error_none);
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
            LOG_ERROR(SERVICE_SENSOR, "Unimplemented opcode for Sensor server 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }

    void sensor_client_session::query_channels(eka2l1::service::ipc_context *ctx) {
        std::optional<channel_info> search_cond = ctx->get_argument_data_from_descriptor<channel_info>(0);
        channel_info *list = reinterpret_cast<channel_info*>(ctx->get_descriptor_argument_ptr(1));
        
        if (!search_cond.has_value() || !list) {
            ctx->complete(epoc::error_argument);
            return;
        }

        drivers::sensor_driver *ssdriver = ctx->sys->get_sensor_driver();
        drivers::sensor_info search_info_driver;

        // TODO: I don't copy vendor and location in because.. don't feel like needed. In future cases, please do.
        // I am just afraid it will break stuffs.
        if (search_cond->channel_data_type_id != 0) {
            search_info_driver.data_type_ = static_cast<drivers::sensor_data_type>(search_cond->channel_data_type_id);
        }

        if (search_cond->channel_type != 0) {
            search_info_driver.type_ = static_cast<drivers::sensor_type>(search_cond->channel_type);
        }

        if (search_cond->quantity != 0) {
            search_info_driver.quantity_ = static_cast<drivers::sensor_data_quantity>(search_cond->quantity);
        }

        std::vector<drivers::sensor_info> infos = ssdriver->queries_active_sensor(search_info_driver);
        std::uint32_t channel_info_count = static_cast<std::uint32_t>(infos.size());
        if (!infos.empty()) {
            for (std::size_t i = 0; i < infos.size(); i++) {
                list[i].channel_id = infos[i].id_;
                list[i].channel_type = infos[i].type_;
                list[i].context_type = 0;
                list[i].location.assign(nullptr, infos[i].location_);
                list[i].vendor_id.assign(nullptr, infos[i].vendor_);
                list[i].quantity = infos[i].quantity_;
            }
        }

        ctx->write_data_to_descriptor_argument(2, channel_info_count);
        ctx->set_descriptor_argument_length(1, channel_info_count * sizeof(channel_info));
        ctx->complete(epoc::error_none);
    }

    void sensor_client_session::open_channel(eka2l1::service::ipc_context *ctx) {
        std::uint32_t channel_id = *(ctx->get_argument_value<std::uint32_t>(0));
        if (channels_.find(channel_id) != channels_.end()) {
            LOG_ERROR(SERVICE_SENSOR, "Channel with ID {} is already opened!", channel_id);
            ctx->complete(epoc::error_already_exists);

            return;
        }

        drivers::sensor_driver *ssdriver = ctx->sys->get_sensor_driver();

        auto controller = ssdriver->new_sensor_controller(channel_id);
        if (!controller) {
            LOG_ERROR(SERVICE_SENSOR, "Failed to instantiate a new sensor controller for channel!");
            ctx->complete(epoc::error_general);

            return;
        }

        // Should always be like this hopefully
        std::uint32_t max_buffer = 4;
        std::uint32_t data_item_size = controller->data_packet_size();

        ctx->write_data_to_descriptor_argument(1, max_buffer);
        ctx->write_data_to_descriptor_argument(2, data_item_size);

        channels_.emplace(channel_id, std::make_unique<sensor_channel>(controller));
        ctx->complete(epoc::error_none);
    }

    void sensor_client_session::close_channel(eka2l1::service::ipc_context *ctx) {
        std::uint32_t channel_id = *(ctx->get_argument_value<std::uint32_t>(0));
        auto find_result = channels_.find(channel_id);
        
        if (find_result != channels_.end()) {
            channels_.erase(find_result);
            ctx->complete(epoc::error_none);

            return;
        }

        LOG_ERROR(SERVICE_SENSOR, "Channel with ID {} is not yet opened!", channel_id);
        ctx->complete(epoc::error_not_ready);
    }

    sensor_channel *sensor_client_session::get_sensor_channel(const std::uint32_t id) {
        auto result = channels_.find(id);
        if (result != channels_.end()) {
            return result->second.get();
        }

        return nullptr;
    }

    void sensor_client_session::start_listening(eka2l1::service::ipc_context *ctx) {
        std::uint32_t channel_id = *(ctx->get_argument_value<std::uint32_t>(0));
        std::optional<listening_parameters> params = ctx->get_argument_data_from_descriptor<listening_parameters>(1);

        if (!params.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        sensor_channel *channel = get_sensor_channel(channel_id);
        if (!channel) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        if (!channel->listen_for_data(params.value())) {
            LOG_ERROR(SERVICE_SENSOR, "Failed to listen for channel data!");
            ctx->complete(epoc::error_general);

            return;
        }

        ctx->complete(epoc::error_none);
    }

    void sensor_client_session::stop_listening(eka2l1::service::ipc_context *ctx) {
        std::uint32_t channel_id = *(ctx->get_argument_value<std::uint32_t>(0));

        sensor_channel *channel = get_sensor_channel(channel_id);
        if (!channel) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        channel->cancel_data_listening();
        ctx->complete(epoc::error_none);
    }

    static void copy_driver_sensor_property_to_client(drivers::sensor_property_data &source, sensor_property &dest) {
        switch (source.data_type_) {
        case drivers::sensor_property_data::DATA_TYPE_BUFFER: {
            dest.property_type = property_types::buffer_property;
            dest.buf_value.assign(nullptr, source.buffer_data_);
            break;
        }

        case drivers::sensor_property_data::DATA_TYPE_INT: {
            dest.property_type = property_types::int_property;
            dest.int_value_min = source.min_int_value_;
            dest.int_value_max = source.max_int_value_;
            dest.int_value = source.int_value_;

            break;
        }

        case drivers::sensor_property_data::DATA_TYPE_DOUBLE: {
            dest.property_type = property_types::real_property;
            dest.real_value_min = source.min_float_value_;
            dest.real_value_max = source.max_float_value_;
            dest.real_value = source.float_value_;

            break;
        }
        }

        dest.array_index = static_cast<std::int16_t>(source.array_index_);
        dest.item_index = source.item_index_;
        dest.property_id = source.property_id_;
    }

    void sensor_client_session::get_property(eka2l1::service::ipc_context *ctx) {
        std::uint32_t channel_id = *(ctx->get_argument_value<std::uint32_t>(0));
        std::optional<sensor_property> res_property = ctx->get_argument_data_from_descriptor<sensor_property>(1);

        if (!res_property.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        sensor_channel *channel = get_sensor_channel(channel_id);
        if (!channel) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        drivers::sensor *controller = channel->get_sensor_controller();
        if (!controller) {
            LOG_INFO(SERVICE_SENSOR, "Weird behaviour: controller is null");
            ctx->complete(epoc::error_general);

            return;
        }

        drivers::sensor_property_data data_result;
        std::int32_t array_index = static_cast<std::int32_t>(res_property->array_index);

        if (!controller->get_property(static_cast<drivers::sensor_property>(res_property->property_id),
            res_property->item_index, &array_index, data_result)) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        copy_driver_sensor_property_to_client(data_result, res_property.value());

        ctx->write_data_to_descriptor_argument(1, res_property.value());
        ctx->complete(epoc::error_none);
    }

    void sensor_client_session::get_all_properties(eka2l1::service::ipc_context *ctx) {
        std::uint32_t channel_id = *(ctx->get_argument_value<std::uint32_t>(0));
        sensor_property *list = reinterpret_cast<sensor_property *>(ctx->get_descriptor_argument_ptr(1));

        if (!list) {
            ctx->complete(epoc::error_argument);
            return;
        }
        
        sensor_channel *channel = get_sensor_channel(channel_id);
        if (!channel) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        drivers::sensor *controller = channel->get_sensor_controller();
        if (!controller) {
            LOG_INFO(SERVICE_SENSOR, "Weird behaviour: controller is null");
            ctx->complete(epoc::error_general);

            return;
        }

        std::vector<drivers::sensor_property_data> properties = controller->get_all_properties(nullptr);
        std::uint32_t property_count = static_cast<std::uint32_t>(properties.size());

        for (size_t i = 0; i < property_count; i++) {
            copy_driver_sensor_property_to_client(properties[i], list[i]);
        }

        ctx->write_data_to_descriptor_argument(2, property_count);
        ctx->set_descriptor_argument_length(1, property_count * sizeof(sensor_property));
        ctx->complete(epoc::error_none);
    }
}
