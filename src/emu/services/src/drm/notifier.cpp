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

#include <epoc/epoc.h>
#include <services/drm/notifier.h>

#include <utils/err.h>

namespace eka2l1 {
    drm_notifier_server::drm_notifier_server(eka2l1::system *sys)
        : service::typical_server(sys, "!DRMNotifier") {
    }

    void drm_notifier_server::connect(service::ipc_context &context) {
        create_session<drm_notifier_client_session>(&context);
        context.complete(epoc::error_none);
    }

    drm_notifier_client_session::drm_notifier_client_session(service::typical_server *serv, const std::uint32_t ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void drm_notifier_client_session::fetch(service::ipc_context *ctx) {
        LOG_ERROR("Unimplemented opcode for DRMNotifier server 0x{:X}", ctx->msg->function);
        ctx->complete(epoc::error_none);
    }
}
