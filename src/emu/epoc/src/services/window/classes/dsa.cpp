/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <epoc/services/window/classes/dsa.h>
#include <epoc/services/window/op.h>

namespace eka2l1::epoc {
    dsa::dsa(window_server_client_ptr client)
        : window_client_obj(client, nullptr) {
    };
    
    void dsa::execute_command(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) {
        ws_dsa_op op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        default: {
            LOG_ERROR("Unimplemented DSA opcode {}", cmd.header.op);
            break;
        }
        }
    }
}
