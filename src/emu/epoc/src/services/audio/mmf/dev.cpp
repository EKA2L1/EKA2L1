/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <epoc/services/audio/mmf/dev.h>
#include <epoc/utils/err.h>
#include <common/log.h>

namespace eka2l1 {
    static const char *MMF_DEV_SERVER_NAME = "!MMFDevServer";

    mmf_dev_server::mmf_dev_server(eka2l1::system *sys)
        : service::typical_server(sys, MMF_DEV_SERVER_NAME) {
    }
    
    void mmf_dev_server::connect(service::ipc_context &context) {
        create_session<mmf_dev_server_session>(&context);
        context.set_request_status(epoc::error_none);
    }
    
    mmf_dev_server_session::mmf_dev_server_session(service::typical_server *serv, service::uid client_ss_uid, epoc::version client_version)
        : service::typical_session(serv, client_ss_uid, client_version) {
    }

    void mmf_dev_server_session::fetch(service::ipc_context *ctx) {
        LOG_ERROR("Unimplemented MMF dev server session opcode {}", ctx->msg->function);
    }
}