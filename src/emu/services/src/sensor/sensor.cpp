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
#include <atomic>

namespace eka2l1 {
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

        case sensor_async_channel_data:
            channel_data(ctx);
            break;

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
                list[i].data_item_size = infos[i].item_size_;
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

        channels_.emplace(channel_id, std::move(controller));
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

    drivers::sensor *sensor_client_session::get_sensor_channel(const std::uint32_t id) {
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

        drivers::sensor *channel = get_sensor_channel(channel_id);
        if (!channel) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        if (!channel->listen_for_data(params->desired_buffering_count, params->maximum_buffering_count,
                                      params->buffering_period)) {
            LOG_ERROR(SERVICE_SENSOR, "Failed to listen for channel data!");
            ctx->complete(epoc::error_general);

            return;
        }

        ctx->complete(epoc::error_none);
    }

    void sensor_client_session::stop_listening(eka2l1::service::ipc_context *ctx) {
        std::uint32_t channel_id = *(ctx->get_argument_value<std::uint32_t>(0));
        drivers::sensor *channel = get_sensor_channel(channel_id);
        if (!channel) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        channel->cancel_data_listening();

        auto ite = channel_data_msgs_.find(channel_id);
        if (ite != channel_data_msgs_.end()) {
            ite->second->complete(epoc::error_cancel);
            channel_data_msgs_.erase(ite);
        }

        ctx->complete(epoc::error_none);
    }

    void sensor_client_session::channel_data(eka2l1::service::ipc_context *ctx) {
        std::uint32_t channel_id = *(ctx->get_argument_value<std::uint32_t>(0));
        drivers::sensor *channel = get_sensor_channel(channel_id);
        if (!channel) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        auto channel_msg_ite = channel_data_msgs_.find(channel_id);

        if (channel_msg_ite != channel_data_msgs_.end()) {
            ctx->complete(epoc::error_in_use);
            return;
        }

        channel_data_msgs_[channel_id] = ctx->move_to_new();
        ask_recv_time_[channel_id] = common::get_current_utc_time_in_microseconds_since_epoch();

        channel->receive_data([this, channel_id](std::vector<std::uint8_t> &data, std::size_t packet_sent) {
            complete_channel_data_request(channel_id, data, packet_sent);
        });
    }

    void sensor_client_session::complete_channel_data_request(const std::uint32_t channel_id, std::vector<std::uint8_t> &data, std::size_t packet_sent) {
        auto channel_msg_ite = channel_data_msgs_.find(channel_id);
        if (channel_msg_ite == channel_data_msgs_.end()) {
            LOG_ERROR(SERVICE_SENSOR, "Channel data receive message is not available!");
            return;
        }

        std::unique_ptr<service::ipc_context> context = std::move(channel_msg_ite->second);
        drivers::sensor *controller = get_sensor_channel(channel_id);

        if (!controller) {
            LOG_ERROR(SERVICE_SENSOR, "Channel controller is not available!");
            return;
        }

        channel_data_msgs_.erase(channel_msg_ite);

        data_count_ret_val ret_val;
        ret_val.item_count_ = packet_sent;
        ret_val.lost_count_ = 0;

        const std::size_t max_write_bytes = context->get_argument_max_data_size(1);
        const std::size_t max_item_fill = max_write_bytes / controller->data_packet_size();

        if (max_item_fill < packet_sent) {
            ret_val.lost_count_ = packet_sent - max_item_fill;
        }

        kernel_system *kern = context->msg->own_thr->get_kernel_object_owner();
        kern->lock();

        context->write_data_to_descriptor_argument(2, ret_val);
        context->write_data_to_descriptor_argument(1, data.data(), common::min<std::uint32_t>(
                static_cast<std::uint32_t>(max_write_bytes), static_cast<std::uint32_t>(data.size())));

        const std::uint64_t now = common::get_current_utc_time_in_microseconds_since_epoch();
        const std::uint64_t passed = (now - ask_recv_time_[channel_id]);

        static constexpr std::uint64_t TIME_GET_BACK_REQUEST = 60;

        // Sometimes we just aligned with when listening was done. So we must delay a bit in case
        // for the active object to be activated. In non-SMP case, finish early may corrupt the
        // status flags and then panic 46!
        if (passed < TIME_GET_BACK_REQUEST) {
            std::this_thread::sleep_for(std::chrono::microseconds(TIME_GET_BACK_REQUEST  - passed));
        }

        context->complete(epoc::error_none);
        kern->unlock();
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

        drivers::sensor *controller = get_sensor_channel(channel_id);
        if (!controller) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        drivers::sensor_property_data data_result;
        std::int32_t array_index = static_cast<std::int32_t>(res_property->array_index);

        if (!controller->get_property(static_cast<drivers::sensor_property>(res_property->property_id),
            res_property->item_index, array_index, data_result)) {
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

        drivers::sensor *controller = get_sensor_channel(channel_id);
        if (!controller) {
            ctx->complete(epoc::error_not_found);
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
