/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <epoc/services/akn/skin/server.h>
#include <epoc/services/akn/skin/ops.h>

#include <common/e32inc.h>
#include <common/log.h>
#include <e32err.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>

namespace eka2l1 {
    akn_skin_server_session::akn_skin_server_session(service::typical_server *svr, service::uid client_ss_uid) 
        : service::typical_session(svr, client_ss_uid) {
    }

    void akn_skin_server_session::do_set_notify_handler(service::ipc_context *ctx) {
        // The notify handler does nothing rather than gurantee that the client already has a handle mechanic
        // to the request notification later.
        client_handler_ = static_cast<std::uint32_t>(*ctx->get_arg<int>(0));
        ctx->set_request_status(KErrNone);
    }

    void akn_skin_server_session::do_next_event(service::ipc_context *ctx) {
        if (flags & ASS_FLAG_CANCELED) {
            // Clear the nof list
            while (!nof_list_.empty()) {
                nof_list_.pop();
            }

            // Set the notifier and both the next one getting the event to cancel
            ctx->set_request_status(KErrCancel);
            nof_info_.complete(KErrCancel);

            return;
        }

        // Check the notify list count
        if (nof_list_.size() > 0) {
            // The notification list is not empty.
            // Take the first element in the queue, and than signal the client with that code.
            const epoc::akn_skin_server_change_handler_notification nof_code = std::move(nof_list_.front());
            nof_list_.pop();

            ctx->set_request_status(static_cast<int>(nof_code));
        } else {
            // There is no notification pending yet.
            // We have to wait for it, so let's get this client as the one to signal.
            nof_info_.requester = ctx->msg->own_thr;
            nof_info_.sts = ctx->msg->request_sts;
        }
    }

    void akn_skin_server_session::do_cancel(service::ipc_context *ctx) {
        // If a handler is set and no pending notifications
        if (client_handler_ && nof_list_.empty()) {
            nof_info_.complete(KErrCancel);
        }

        ctx->set_request_status(KErrNone);
    }
    
    void akn_skin_server_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case epoc::akn_skin_server_set_notify_handler: {
            do_set_notify_handler(ctx);
            break;
        }

        case epoc::akn_skin_server_next_event: {
            do_next_event(ctx);
            break;
        }

        case epoc::akn_skin_server_cancel: {
            do_cancel(ctx);
            break;
        }
        
        default: {
            LOG_ERROR("Unimplemented opcode: {}", epoc::akn_skin_server_opcode_to_str(
                static_cast<const epoc::akn_skin_server_opcode>(ctx->msg->function)));

            break;
        }
        }
    }

    akn_skin_server::akn_skin_server(eka2l1::system *sys) 
        : service::typical_server(sys, "!AknSkinServer")
        , settings_(nullptr) {
    }

    void akn_skin_server::connect(service::ipc_context &ctx) {
        if (!settings_) {
            do_initialisation();
        }

        create_session<akn_skin_server_session>(&ctx);
        ctx.set_request_status(KErrNone);
    }

    void akn_skin_server::do_initialisation() {
        kernel_system *kern = sys->get_kernel_system();
        server_ptr svr = kern->get_by_name<service::server>("!CentralRepository");
        
        // Older versions dont use cenrep.
        settings_ = std::make_unique<epoc::akn_ss_settings>(sys->get_io_system(), !svr ? nullptr :
            reinterpret_cast<central_repo_server*>(&(*svr)));

        // Create skin chunk
        skin_chunk_ = kern->create_and_add<kernel::chunk>(kernel::owner_type::kernel,
            sys->get_memory_system(), nullptr, "AknsSrvSharedMemoryChunk",
            0, 160 * 1024, 384 * 1024, prot::read_write, kernel::chunk_type::normal, 
            kernel::chunk_access::global, kernel::chunk_attrib::none).second;

        // Create semaphores and mutexes
        skin_chunk_sema_ = kern->create_and_add<kernel::semaphore>(kernel::owner_type::kernel, 
            "AknsSrvWaitSemaphore", 127, kernel::access_type::global_access).second;

        // Render mutex. Use when render skins
        skin_chunk_render_mut_ = kern->create_and_add<kernel::mutex>(kernel::owner_type::kernel,
            sys->get_timing_system(), "AknsSrvRenderSemaphore", false, 
            kernel::access_type::global_access).second;
    }
}
