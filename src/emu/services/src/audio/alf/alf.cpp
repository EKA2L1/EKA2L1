/*
 * Copyright (c) 2022 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
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

#include <services/audio/alf/alf.h>
#include <system/epoc.h>

#include <utils/err.h>

namespace eka2l1 {
    alf_streamer_server::alf_streamer_server(eka2l1::system *sys)
        : service::typical_server(sys, "alfstreamerserver") {
    }

    void alf_streamer_server::connect(service::ipc_context &context) {
        create_session<alf_streamer_session>(&context);
        context.complete(epoc::error_none);
    }

    alf_streamer_session::alf_streamer_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void alf_streamer_session::fetch(service::ipc_context *ctx) {
        int function = ctx->msg->function;
        if ((function >= 20000) && (function <= 20005)) {
            function -= 20000;
        }

        switch (function) {
        case 2: {
            // Synchronous effect-plugin calls return a serialized TInt error
            // in argument 2. Effects are intentionally disabled, so success
            // is sufficient for all no-op transition commands.
            const std::int32_t result = epoc::error_none;
            ctx->write_data_to_descriptor_argument(2, result);
            ctx->complete(epoc::error_none);
            break;
        }
        case 3:
            // This is a long-lived transition-policy notification. The client
            // fetches policy data only after the request is completed.
            if (pending_plugin_request_) {
                pending_plugin_request_->complete(epoc::error_cancel);
            }
            pending_plugin_request_ = ctx->move_to_new();
            break;
        case 4:
            if (pending_plugin_request_) {
                pending_plugin_request_->complete(epoc::error_cancel);
                pending_plugin_request_.reset();
            }
            ctx->complete(epoc::error_none);
            break;
        default:
            ctx->complete(epoc::error_none);
            break;
        }
    }
}
