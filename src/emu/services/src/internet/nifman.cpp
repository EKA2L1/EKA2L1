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

#include <system/epoc.h>
#include <services/internet/nifman.h>

#include <utils/err.h>
#include <common/log.h>

namespace eka2l1 {
    nifman_server::nifman_server(eka2l1::system *sys)
        : service::typical_server(sys, "NifmanServer") {
    }

    void nifman_server::connect(service::ipc_context &context) {
        create_session<nifman_client_session>(&context);
        context.complete(epoc::error_none);
    }

    nifman_client_session::nifman_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void nifman_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case nifman_get_active_int_setting:
            ctx->complete(epoc::error_not_ready);
            return;

        default:
            LOG_ERROR(SERVICE_NIFMAN, "Unimplemented opcode for NifMan 0x{:X}, complete all", ctx->msg->function);
            ctx->complete(epoc::error_none);

            break;
        }
    }
}
