/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <services/goommonitor/goommonitor.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1 {
    goom_monitor_server::goom_monitor_server(eka2l1::system *sys)
        : service::typical_server(sys, "GOomMonitorServer") {
    }

    void goom_monitor_server::connect(service::ipc_context &context) {
        create_session<goom_monitor_session>(&context);
        context.complete(epoc::error_none);
    }

    goom_monitor_session::goom_monitor_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void goom_monitor_session::request_free_memory(service::ipc_context *ctx) {
        std::optional<std::uint32_t> bytes_requested = ctx->get_argument_value<std::uint32_t>(0);
        if (bytes_requested.has_value()) {
            LOG_TRACE(SERVICE_GOOMMONITOR, "Application requested to have {}B of free memory!");
        }

        ctx->complete(epoc::error_none);
    }

    void goom_monitor_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case goom_monitor_request_free_memory:
            request_free_memory(ctx);
            break;

        default:
            LOG_ERROR(SERVICE_GOOMMONITOR, "Unimplemented opcode for Global OOM Monitor server 0x{:X}", ctx->msg->function);
            break;
        }

        ctx->complete(epoc::error_none);
    }
}
