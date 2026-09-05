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
    // A backend callback can outlive the request that armed it, and the session it
    // would report to. The callback holds this instead of the session; session_ is
    // only read or cleared while the kernel lock is held.
    struct sensor_client_session_callback_state {
        kernel_system *kern_;
        sensor_client_session *session_;
    };

    sensor_server::sensor_server(eka2l1::system *sys)
        : service::typical_server(sys, "!SensorServer") {
    }

    void sensor_server::connect(service::ipc_context &context) {
        create_session<sensor_client_session>(&context);
        context.complete(epoc::error_none);
    }

    sensor_client_session::sensor_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version)
        , callback_state_(std::make_shared<sensor_client_session_callback_state>(
              sensor_client_session_callback_state{ serv->get_kernel_object_owner(), this })) {
    }

    sensor_client_session::~sensor_client_session() {
        // Sessions are destroyed under the kernel lock, so a callback that is already
        // waiting on it sees this before it can look at the session.
        callback_state_->session_ = nullptr;

        for (auto &channel : channels_) {
            channel.second->cancel_data_listening();
        }
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
        if (!ssdriver) {
            // A frontend that has no sensor backend leaves the driver unset. Report
            // no channels rather than dereferencing it.
            std::uint32_t channel_info_count = 0;
            ctx->write_data_to_descriptor_argument(2, channel_info_count);
            ctx->set_descriptor_argument_length(1, 0);
            ctx->complete(epoc::error_none);
            return;
        }

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
        if (!ssdriver) {
            LOG_ERROR(SERVICE_SENSOR, "No sensor driver available, unable to open channel {}", channel_id);
            ctx->complete(epoc::error_not_supported);

            return;
        }

        auto controller = ssdriver->new_sensor_controller(channel_id);
        if (!controller) {
            LOG_ERROR(SERVICE_SENSOR, "Failed to instantiate a new sensor controller for channel!");
            ctx->complete(epoc::error_general);

            return;
        }

        std::uint32_t max_buffer = SENSOR_MAX_BUFFERING_COUNT;
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
            auto data_msg = channel_data_msgs_.find(channel_id);
            if (data_msg != channel_data_msgs_.end()) {
                data_msg->second->complete(epoc::error_cancel);
                channel_data_msgs_.erase(data_msg);
            }
            ask_recv_time_.erase(channel_id);

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

        params->normalize_buffering_counts(SENSOR_MAX_BUFFERING_COUNT);
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
        ask_recv_time_.erase(channel_id);

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

        // Both output descriptors are mandatory here. Refusing a bad one now is much
        // better than arming a backend callback that cannot report anywhere later.
        if (!ctx->get_descriptor_argument_ptr(1) || !ctx->get_descriptor_argument_ptr(2)
            || (ctx->get_argument_max_data_size(2) < sizeof(data_count_ret_val))) {
            ctx->complete(epoc::error_bad_descriptor);
            return;
        }

        channel_data_msgs_[channel_id] = ctx->move_to_new();
        ask_recv_time_[channel_id] = common::get_current_utc_time_in_microseconds_since_epoch();

        const std::shared_ptr<sensor_client_session_callback_state> callback_state = callback_state_;
        channel->receive_data([callback_state, channel_id](std::vector<std::uint8_t> &data, std::size_t packet_sent) {
            kernel_system *kern = callback_state->kern_;
            kern->lock();

            // Taking the kernel lock before touching the session or the IPC message
            // serialises this completion against StopListening, CloseChannel and
            // session teardown, all of which run on the emulated thread. The lock
            // does not help during wipeout, though: the session is still reachable
            // while the threads and the scheduler behind it are already gone, so
            // completing would dewait a thread that no longer has scheduler state.
            // Same reason session::disconnect() checks it before signalling.
            if (!kern->is_wiping() && callback_state->session_) {
                callback_state->session_->complete_channel_data_request_locked(kern, channel_id, data, packet_sent);
            }

            kern->unlock();
        });
    }

    void sensor_client_session::complete_channel_data_request_locked(kernel_system *kern,
        const std::uint32_t channel_id, std::vector<std::uint8_t> &data, std::size_t packet_sent) {
        auto channel_msg_ite = channel_data_msgs_.find(channel_id);
        if (channel_msg_ite == channel_data_msgs_.end()) {
            return;
        }

        std::unique_ptr<service::ipc_context> context = std::move(channel_msg_ite->second);
        channel_data_msgs_.erase(channel_msg_ite);

        drivers::sensor *controller = get_sensor_channel(channel_id);

        if (!controller) {
            ask_recv_time_.erase(channel_id);
            return;
        }

        // The message holds a raw pointer to the requesting thread, which may have
        // gone away while the backend was working. The kernel lock is held here.
        if (!context || !context->msg || !kern->is_thread_alive(context->msg->own_thr)) {
            ask_recv_time_.erase(channel_id);
            return;
        }

        if (!context->get_descriptor_argument_ptr(1) || !context->get_descriptor_argument_ptr(2)) {
            ask_recv_time_.erase(channel_id);
            context->complete(epoc::error_bad_descriptor);
            return;
        }

        data_count_ret_val ret_val;
        ret_val.item_count_ = packet_sent;
        ret_val.lost_count_ = 0;

        const std::size_t max_write_bytes = context->get_argument_max_data_size(1);
        const std::size_t count_write_bytes = context->get_argument_max_data_size(2);
        if (count_write_bytes < sizeof(ret_val)) {
            ask_recv_time_.erase(channel_id);
            context->complete(epoc::error_overflow);
            return;
        }

        const std::size_t max_item_fill = max_write_bytes / controller->data_packet_size();

        if (max_item_fill < packet_sent) {
            ret_val.lost_count_ = packet_sent - max_item_fill;
        }

        const bool count_written = context->write_data_to_descriptor_argument(2, ret_val);
        const bool data_written = context->write_data_to_descriptor_argument(1, data.data(), common::min<std::uint32_t>(
            static_cast<std::uint32_t>(max_write_bytes), static_cast<std::uint32_t>(data.size())));

        if (!count_written || !data_written) {
            ask_recv_time_.erase(channel_id);
            context->complete(epoc::error_bad_descriptor);
            return;
        }

        const std::uint64_t now = common::get_current_utc_time_in_microseconds_since_epoch();
        const auto ask_time = ask_recv_time_.find(channel_id);
        const std::uint64_t passed = (ask_time != ask_recv_time_.end()) ? (now - ask_time->second) : 0;
        ask_recv_time_.erase(channel_id);

        static constexpr std::uint64_t TIME_GET_BACK_REQUEST = 60;

        // Sometimes we just aligned with when listening was done. So we must delay a bit in case
        // for the active object to be activated. In non-SMP case, finish early may corrupt the
        // status flags and then panic 46!
        if (passed < TIME_GET_BACK_REQUEST) {
            std::this_thread::sleep_for(std::chrono::microseconds(TIME_GET_BACK_REQUEST  - passed));
        }

        context->complete(epoc::error_none);
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
