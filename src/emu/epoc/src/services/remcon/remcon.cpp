/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <epoc/services/remcon/remcon.h>
#include <epoc/utils/err.h>
#include <common/log.h>

namespace eka2l1 {
    remcon_server::remcon_server(eka2l1::system *sys)
        : service::typical_server(sys, "!RemConSrv") {

    }

    void remcon_server::connect(service::ipc_context &ctx) {
        create_session<remcon_session>(&ctx);
        ctx.set_request_status(epoc::error_none);
    }
    
    remcon_session::remcon_session(service::typical_server *svr, service::uid client_ss_uid, epoc::version client_ver)
        : service::typical_session(svr, client_ss_uid, client_ver) {

    }

    void remcon_session::fetch(service::ipc_context *ctx) {
        LOG_ERROR("Unimplemented remcon opcode {}", ctx->msg->function);
    }
}