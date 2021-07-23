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

#include <services/comm/comm.h>
#include <system/epoc.h>

#include <kernel/property.h>
#include <utils/err.h>

namespace eka2l1 {
    std::string get_comm_server_name_by_epocver(const epocver ver) {
        if (ver <= epocver::eka2) {
            return "CommServer";
        }

        return "!CommServer";
    }

    static constexpr std::uint32_t C32START_FIRST_UID = 0x101F7988;

    comm_server::comm_server(eka2l1::system *sys)
        : service::typical_server(sys, get_comm_server_name_by_epocver(sys->get_symbian_version_use()))
        , c32start_prop_(nullptr) {
        c32start_prop_ = kern->create<service::property>();
        c32start_prop_->first = C32START_FIRST_UID;
        c32start_prop_->second = 1;

        // On S60v2 it will keep spin loop until this value reach larger then 9. Not sure what it is...
        c32start_prop_->define(service::property_type::int_data, 4);
        c32start_prop_->set_int(10);
    }

    void comm_server::connect(service::ipc_context &context) {
        create_session<comm_client_session>(&context);
        context.complete(epoc::error_none);
    }

    comm_client_session::comm_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void comm_client_session::fetch(service::ipc_context *ctx) {
        LOG_ERROR(SERVICE_ECOMM, "Unimplemented opcode for Comm server 0x{:X}", ctx->msg->function);
        ctx->complete(epoc::error_none);
    }
}
