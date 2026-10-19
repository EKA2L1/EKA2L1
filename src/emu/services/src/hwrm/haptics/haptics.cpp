/*
 * Copyright (c) 2026 EKA2L1 Team
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

#include <services/hwrm/haptics/haptics.h>
#include <kernel/timing.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1 {
    using namespace epoc::haptics;

    static int write_reply(service::ipc_context &ctx, const int slot,
        const std::uint8_t *data, const std::size_t size) {
        if (ctx.msg->args.get_arg_type(slot) != ipc_arg_type::des8 || !ctx.get_descriptor_argument_ptr(slot)) {
            return epoc::error_bad_descriptor;
        }
        if (ctx.get_argument_max_data_size(slot) < size) return epoc::error_overflow;
        return ctx.write_data_to_descriptor_argument(slot, data, size) ? epoc::error_none : epoc::error_bad_descriptor;
    }

    static int write_integer(service::ipc_context &ctx, const int slot, const std::int32_t value) {
        return write_reply(ctx, slot, reinterpret_cast<const std::uint8_t *>(&value), sizeof(value));
    }

    haptics_server::haptics_server(system *sys)
        : service::typical_server(sys, "!hwrmhapticsserver") {
    }

    haptics_server::~haptics_server() {
        shutdown();
    }

    void haptics_server::shutdown() {
        reservations_.clear();
        output_owner_ = nullptr;
        if (vibrator_) vibrator_->stop_vibrate();
        clear_all_sessions();
    }

    int haptics_server::destroy() {
        shutdown();
        return service::typical_server::destroy();
    }

    void haptics_server::connect(service::ipc_context &ctx) {
        create_session<haptics_session>(&ctx);
        ctx.complete(epoc::error_none);
    }

    void haptics_server::update_output(haptics_session *session, const std::uint64_t now) {
        const auto output = status(session) == 0 ? session->device_.output(now) : vibration{};
        if (!output.duration || !output.intensity) {
            if (output_owner_ == session) {
                if (vibrator_) vibrator_->stop_vibrate();
                output_owner_ = nullptr;
            }
            return;
        }
        if (output_owner_ && output_owner_ != session
            && output_owner_->device_.output(now).duration
            && output_owner_->device_.priority() > session->device_.priority()) {
            return;
        }
        output_owner_ = session;
        if (!vibrator_) vibrator_ = drivers::hwrm::make_suitable_vibrator();
        if (vibrator_) vibrator_->vibrate(output.duration, output.intensity);
    }

    void haptics_server::detach(haptics_session *session) {
        if (output_owner_ == session) {
            if (vibrator_) vibrator_->stop_vibrate();
            output_owner_ = nullptr;
        }
        release(session);
    }

    void haptics_server::notify_reservations() {
        for (auto &[id, entry] : sessions) {
            auto client = static_cast<haptics_session *>(entry.get());
            const auto value = status(client);
            if (client->initialized_ && client->last_status_ != value) {
                client->last_status_ = value;
                client->status_changed_ = true;
                client->notify_status();
            }
        }
    }

    void haptics_server::reserve(haptics_session *session) {
        if (std::find(reservations_.begin(), reservations_.end(), session) != reservations_.end()) return;
        // Clients without a trusted policy entry share the default reservation priority, in FIFO order.
        reservations_.push_back(session);
        if (output_owner_ && output_owner_ != reservations_.front()) {
            output_owner_->device_.stop_all();
            if (vibrator_) vibrator_->stop_vibrate();
            output_owner_ = nullptr;
        }
        notify_reservations();
    }

    void haptics_server::release(haptics_session *session) {
        const auto it = std::find(reservations_.begin(), reservations_.end(), session);
        if (it == reservations_.end()) return;
        if (it == reservations_.begin()) {
            session->device_.stop_all();
            if (output_owner_ == session) {
                if (vibrator_) vibrator_->stop_vibrate();
                output_owner_ = nullptr;
            }
        }
        reservations_.erase(it);
        notify_reservations();
    }

    std::int32_t haptics_server::status(haptics_session *session) const {
        if (reservations_.empty() || reservations_.front() == session) return 0;
        return std::find(reservations_.begin(), reservations_.end(), session) == reservations_.end() ? 1 : 2;
    }

    haptics_session::haptics_session(service::typical_server *serv, kernel::uid id, epoc::version version)
        : service::typical_session(serv, id, version) {
    }

    haptics_session::~haptics_session() {
        cleanup();
    }

    void haptics_session::cleanup() {
        if (status_request_) {
            status_request_->complete(epoc::error_cancel);
            status_request_.reset();
        }
        server<haptics_server>()->detach(this);
        device_.reset();
        initialized_ = false;
        status_changed_ = false;
        actuator_changed_ = false;
        actuator_ = 0;
    }

    void haptics_session::notify_status() {
        if (!status_request_) return;
        const auto types = *status_request_->get_argument_value<std::int32_t>(0);
        const bool session_event = status_changed_ && types != 1;
        const bool actuator_event = actuator_changed_ && types != 0;
        if (!session_event && !actuator_event) return;
        auto request = std::move(status_request_);
        const std::int32_t type = session_event ? 0 : 1;
        const std::int32_t value = session_event ? server<haptics_server>()->status(this) : 2;
        int error = write_integer(*request, 1, type);
        if (error == epoc::error_none) error = write_integer(*request, 2, value);
        if (error == epoc::error_none && !session_event) error = write_integer(*request, 3, actuator_);
        if (error == epoc::error_none) {
            if (session_event) status_changed_ = false;
            else actuator_changed_ = false;
        }
        request->complete(error);
    }

    void haptics_session::execute(service::ipc_context *ctx) {
        const auto size = ctx->get_argument_data_size(0);
        const auto data = ctx->get_descriptor_argument_ptr(0);
        if (!data || size < 2 || size > 4096) {
            ctx->complete(epoc::error_argument);
            return;
        }
        const auto cmd = static_cast<vibe_command>(data[0] | (data[1] << 8));
        const auto op = ctx->msg->function;
        const bool is_play = cmd == vibe_command::play_sweep || cmd == vibe_command::play_periodic;
        if ((op == open_actuator) != (cmd == vibe_command::open)
            || (is_play && op != play_effect)) {
            ctx->complete(epoc::error_argument);
            return;
        }
        auto serv = server<haptics_server>();
        if (is_play && serv->status(this) != 0) {
            ctx->complete(serv->status(this) == 2 ? epoc::error_none : epoc::error_in_use);
            return;
        }
        if (op == open_actuator) {
            const auto actuator = *ctx->get_argument_value<std::int32_t>(2);
            if (actuator != 1 && actuator != 2) {
                ctx->complete(epoc::error_not_supported);
                return;
            }
        }
        const auto now = ctx->sys->get_ntimer()->microseconds() / 1000;
        auto candidate = device_;
        const auto result = candidate.execute(std::vector<std::uint8_t>(data, data + size), now);
        if (result.error != epoc::error_none) {
            ctx->complete(result.error);
            return;
        }
        int error = epoc::error_none;
        if (!result.data.empty()) {
            error = write_reply(*ctx, 1, result.data.data(), result.data.size());
            if (error != epoc::error_none) {
                ctx->complete(error);
                return;
            }
        }
        device_ = std::move(candidate);
        if (op == open_actuator) {
            actuator_ = *ctx->get_argument_value<std::int32_t>(2);
            actuator_changed_ = true;
            notify_status();
        } else if (cmd != vibe_command::get_property && cmd != vibe_command::capability
            && cmd != vibe_command::effect_state) {
            serv->update_output(this, now);
        }
        ctx->complete(epoc::error_none);
    }

    void haptics_session::fetch(service::ipc_context *ctx) {
        const auto op = ctx->msg->function;
        if (op == create_service) {
            if (initialized_) ctx->complete(epoc::error_already_exists);
            else {
                initialized_ = true;
                last_status_ = server<haptics_server>()->status(this);
                status_changed_ = true;
                ctx->complete(epoc::error_none);
            }
            return;
        }
        if (!initialized_) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }
        switch (op) {
        case command:
        case play_effect:
        case open_actuator:
        case set_license:
            execute(ctx);
            break;
        case supported_actuators:
            ctx->complete(write_integer(*ctx, 0, 3));
            break;
        case get_status:
            ctx->complete(write_integer(*ctx, 0, server<haptics_server>()->status(this)));
            break;
        case status_notification: {
            const auto types = *ctx->get_argument_value<std::int32_t>(0);
            if (types < 0 || types > 2) ctx->complete(epoc::error_argument);
            else if (status_request_) ctx->complete(epoc::error_in_use);
            else {
                status_request_ = ctx->move_to_new();
                notify_status();
            }
            break;
        }
        case reserve:
            if (*ctx->get_argument_value<std::int32_t>(0)) {
                ctx->complete(epoc::error_access_denied);
            } else {
                server<haptics_server>()->reserve(this);
                ctx->complete(epoc::error_none);
            }
            break;
        case release:
            server<haptics_server>()->release(this);
            ctx->complete(epoc::error_none);
            break;
        case epoc::haptics::cleanup:
            this->cleanup();
            ctx->complete(epoc::error_none);
            break;
        default:
            ctx->complete(epoc::error_not_supported);
            break;
        }
    }
}
