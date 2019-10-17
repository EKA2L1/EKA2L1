/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <epoc/services/audio/keysound/keysound.h>

namespace eka2l1 {
    keysound_session::keysound_session(service::typical_server *svr, service::uid client_ss_uid)
        : service::typical_session(svr, client_ss_uid) {
    }

    void keysound_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        default:
            LOG_ERROR("Unimplemented keysound server opcode: {}", ctx->msg->function);
            break;
        }
    }

    static constexpr const char *KEYSOUND_SERVER_NAME = "KeySoundServer";

    keysound_server::keysound_server(system *sys)
        : service::typical_server(sys, KEYSOUND_SERVER_NAME) {
    }
    
    void keysound_server::connect(service::ipc_context &context) {
        create_session<keysound_session>(&context);
        context.set_request_status(0);
    }
}