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

#include <services/internet/nifman.h>
#include <services/socket/server.h>
#include <system/epoc.h>

#include <common/cvt.h>
#include <common/log.h>
#include <utils/err.h>

namespace eka2l1 {
    // Symbian 6.1 reports EConnectionOpen before EIfProgressLinkUp.
    static constexpr std::int32_t nifman_connection_open = 14;
    static constexpr std::int32_t nifman_interface_up = 1000;

    static bool write_progress(service::ipc_context *ctx, const epoc::socket::conn_progress &progress) {
        const bool written = ctx->write_data_to_descriptor_argument(0, progress);
        ctx->complete(written ? epoc::error_none : epoc::error_argument);
        return written;
    }

    nifman_server::nifman_server(eka2l1::system *sys)
        : service::typical_server(sys, "NifmanServer")
        , sock_serv_(nullptr) {
    }

    void nifman_server::connect(service::ipc_context &context) {
        if (!sock_serv_) {
            sock_serv_ = reinterpret_cast<socket_server *>(kern->get_by_name<service::server>(
                get_socket_server_name_by_epocver(kern->get_epoc_version())));
        }

        create_session<nifman_client_session>(&context);
        context.complete(epoc::error_none);
    }

    nifman_client_session::nifman_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version)
        , agent_(nullptr)
        , conn_(nullptr) {
    }

    nifman_client_session::~nifman_client_session() {
        if (progress_notification_
            && progress_notification_->sys->get_kernel_system()->is_thread_alive(progress_notification_->msg->own_thr)) {
            progress_notification_->complete(epoc::error_cancel);
        }
    }

    void nifman_client_session::set_progress(const std::int32_t stage) {
        if (progress_.stage_ == stage) {
            return;
        }

        progress_.stage_ = stage;
        if (progress_notification_) {
            auto notification = std::move(progress_notification_);
            if (notification->sys->get_kernel_system()->is_thread_alive(notification->msg->own_thr)) {
                deliver_progress(notification.get());
            }
        }
    }

    void nifman_client_session::deliver_progress(service::ipc_context *ctx) {
        // Retain agent readiness until observed before exposing interface readiness.
        if (write_progress(ctx, progress_) && progress_.stage_ == nifman_connection_open) {
            set_progress(nifman_interface_up);
        }
    }

    void nifman_client_session::progress_notification(service::ipc_context *ctx) {
        if (progress_notification_) {
            ctx->complete(epoc::error_in_use);
            return;
        }

        if (ctx->get_argument_data_size(0) != sizeof(progress_)
            || ctx->get_argument_max_data_size(0) < sizeof(progress_)) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const auto previous = ctx->get_argument_data_from_descriptor<epoc::socket::conn_progress>(0);
        if (!previous) {
            ctx->complete(epoc::error_argument);
        } else {
            if (previous->stage_ == nifman_connection_open && progress_.stage_ == nifman_connection_open) {
                set_progress(nifman_interface_up);
            }
            if (previous->stage_ != progress_.stage_ || previous->error_ != progress_.error_) {
                deliver_progress(ctx);
            } else {
                progress_notification_ = ctx->move_to_new();
            }
        }
    }

    void nifman_client_session::open(service::ipc_context *ctx) {
        std::optional<std::u16string> agt_name = ctx->get_argument_value<std::u16string>(0);
        if (!agt_name.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        // Find this connect agent in socket server
        socket_server *ssock = server<nifman_server>()->get_socket_server();
        if (!ssock) {
            LOG_ERROR(SERVICE_INTERNET, "Nifman requires HLE Socket server to be present!");
            ctx->complete(epoc::error_not_supported);

            return;
        }

        agent_ = ssock->get_connect_agent(agt_name.value());
        if (!agent_) {
            LOG_ERROR(SERVICE_INTERNET, "Agent with name {} not found!", common::ucs2_to_utf8(agt_name.value()));
            ctx->complete(epoc::error_not_found);

            return;
        }

        ctx->complete(epoc::error_none);
    }

    void nifman_client_session::get_active_settings(service::ipc_context *ctx, const epoc::socket::setting_type type) {
        if (!conn_) {
            // TODO: In morden RConnection docs (the replacement of this server), it said that you can't query
            // settings when no connection is yet to be established... Or no call Start()
            // LOG_ERROR(SERVICE_INTERNET, "No connection has been established to get setting");
            ctx->complete(epoc::error_not_ready);
            return;
        }

        std::optional<std::u16string> table_name = ctx->get_argument_value<std::u16string>(0);
        std::optional<std::u16string> column_name = ctx->get_argument_value<std::u16string>(1);

        if (!table_name.has_value() || !column_name.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const std::u16string setting_name = table_name.value() + u"\\" + column_name.value();

        std::uint8_t *dest_buffer = ctx->get_descriptor_argument_ptr(2);
        std::size_t max_size = ctx->get_argument_max_data_size(2);

        if (!dest_buffer) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const std::size_t res = conn_->get_setting(setting_name, type, dest_buffer, max_size);
        if (res == static_cast<std::size_t>(-1)) {
            LOG_ERROR(SERVICE_INTERNET, "Fail to get setting of a connection!");
            ctx->complete(epoc::error_general);

            return;
        }

        ctx->set_descriptor_argument_length(2, static_cast<std::uint32_t>(res));
        ctx->complete(epoc::error_none);
    }

    void nifman_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case nifman_start:
            if (!agent_) {
                ctx->complete(epoc::error_not_ready);
            } else if (progress_.stage_ == nifman_connection_open || progress_.stage_ == nifman_interface_up) {
                ctx->complete(epoc::error_already_exists);
            } else {
                // Internet sockets already use the host network; no guest dial-up is required.
                set_progress(nifman_connection_open);
                ctx->complete(epoc::error_none);
            }
            break;

        case nifman_open:
            open(ctx);
            break;

        case nifman_stop:
            set_progress(0);
            ctx->complete(epoc::error_none);
            break;

        case nifman_progress:
            deliver_progress(ctx);
            break;

        case nifman_progress_notification:
            progress_notification(ctx);
            break;

        case nifman_cancel_progress_notification:
            if (progress_notification_) {
                if (ctx->sys->get_kernel_system()->is_thread_alive(progress_notification_->msg->own_thr)) {
                    progress_notification_->complete(epoc::error_cancel);
                }
                progress_notification_.reset();
            }
            ctx->complete(epoc::error_none);
            break;

        case nifman_last_progress_error:
            write_progress(ctx, {});
            break;

        case nifman_network_active:
            ctx->complete(progress_.stage_ == nifman_connection_open || progress_.stage_ == nifman_interface_up);
            break;

        case nifman_set_overrides:
            // CommsDB dial-up preferences do not change the host network connection.
            ctx->complete(agent_ ? epoc::error_none : epoc::error_not_ready);
            break;

        case nifman_get_active_int_setting:
            get_active_settings(ctx, epoc::socket::setting_type_int);
            return;

        default:
            LOG_ERROR(SERVICE_INTERNET, "Unimplemented opcode for NifMan 0x{:X}, complete all", ctx->msg->function);
            ctx->complete(epoc::error_none);

            break;
        }
    }
}
