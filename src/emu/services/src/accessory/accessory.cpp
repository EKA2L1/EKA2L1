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

#include <services/accessory/accessory.h>
#include <services/accessory/common.h>

#include <system/epoc.h>

#include <utils/err.h>

namespace eka2l1 {
    accessory_server::accessory_server(eka2l1::system *sys)
        : service::typical_server(sys, "!AccServer") {
    }

    void accessory_server::connect(service::ipc_context &context) {
        create_session<accessory_session>(&context);
        context.complete(epoc::error_none);
    }

    accessory_subsession::accessory_subsession(accessory_server *svr)
        : serv_(svr) {
    }

    accessory_single_connection_subsession::accessory_single_connection_subsession(accessory_server *svr)
        : accessory_subsession(svr) {
    }

    void accessory_single_connection_subsession::get_accessory_connection_status(service::ipc_context *ctx) {
        // NOTE: The third argument named flags is not considered here. Check again if troubles rise.
        LOG_TRACE(SERVICE_ACCESSORY, "GetAccessoryConnectionStatus stubbed to all not available");

        epoc::acc::generic_id_array arr;

        ctx->write_data_to_descriptor_argument<epoc::acc::generic_id_array>(0, arr);
        ctx->complete(epoc::error_none);
    }

    void accessory_single_connection_subsession::notify_new_accessory_connected(service::ipc_context *ctx) {
        LOG_TRACE(SERVICE_ACCESSORY, "Notify new accessory connect stubbed (missing arugments)");
        accessory_connected_nof_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
    }

    void accessory_single_connection_subsession::cancel_notify_new_accessory_connected(service::ipc_context *ctx) {
        accessory_connected_nof_.complete(epoc::error_cancel);
        ctx->complete(epoc::error_none);
    }

    bool accessory_single_connection_subsession::fetch(service::ipc_context *ctx) {
        kernel_system *kern = serv_->get_kernel_object_owner();

        if (kern->get_epoc_version() <= epocver::epoc93fp2) {
            switch (ctx->msg->function) {
            case epoc::acc::opcode_s60v3_notify_new_accessory_connected:
                notify_new_accessory_connected(ctx);
                break;

            case epoc::acc::opcode_s60v3_get_accessory_connection_status:
                get_accessory_connection_status(ctx);
                break;

            case epoc::acc::opcode_s60v3_cancel_notify_new_accessory_connected:
                cancel_notify_new_accessory_connected(ctx);
                break;

            default:
                LOG_ERROR(SERVICE_ACCESSORY, "Unimplemented opcode for Accessory single connection subsession 0x{:X}", ctx->msg->function);
                break;
            }
        }

        return false;
    }

    accessory_session::accessory_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void accessory_session::create_accessory_single_connection_subsession(service::ipc_context *ctx) {
        accessory_subsession_instance inst = std::make_unique<accessory_single_connection_subsession>(server<accessory_server>());
        const std::uint32_t id = static_cast<std::uint32_t>(subsessions_.add(inst));

        ctx->write_data_to_descriptor_argument<std::uint32_t>(3, id);
        ctx->complete(epoc::error_none);
    }

    void accessory_session::fetch(service::ipc_context *ctx) {
        kernel_system *kern = server<accessory_server>()->get_kernel_object_owner();

        if (kern->get_epoc_version() <= epocver::epoc93fp2) {
            switch (ctx->msg->function) {
            case epoc::acc::opcode_s60v3_create_accessory_connection_subsession:
                create_accessory_single_connection_subsession(ctx);
                return;

            default:
                break;
            }
        }

        std::optional<std::uint32_t> handle = ctx->get_argument_value<std::uint32_t>(3);
        if (handle.has_value()) {
            accessory_subsession_instance *inst = subsessions_.get(handle.value());
            if (inst) {
                if ((*inst)->fetch(ctx)) {
                    subsessions_.remove(handle.value());
                }

                return;
            }
        }

        LOG_ERROR(SERVICE_ACCESSORY, "Unimplemented opcode for accessory session 0x{:X}", ctx->msg->function);
    }
}
