/*
 * Copyright (c) 2026 EKA2L1 Team
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

#include <services/memorymanager/memorymanager.h>

#include <system/epoc.h>
#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <utils/err.h>

namespace eka2l1 {
    memory_manager_session::memory_manager_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_ver)
        : service::typical_session(svr, client_ss_uid, client_ver) {
    }

    memory_manager_session::~memory_manager_session() {
        cancel_low_ram_notification();
    }

    void memory_manager_session::cancel_low_ram_notification() {
        auto *kern = server<memory_manager_server>()->get_kernel_object_owner();
        if (!low_ram_nof_.empty() && kern->get_by_id<kernel::thread>(low_ram_requester_)) {
            low_ram_nof_.complete(epoc::error_cancel);
        }
        low_ram_nof_ = epoc::notify_info{};
        low_ram_requester_ = 0;
    }

    void memory_manager_session::fetch(service::ipc_context *context) {
        switch (context->msg->function) {
        case memory_manager_request_free_ram:
        case memory_manager_free_ram_available:
            context->complete(epoc::error_none);
            return;

        case memory_manager_request_free_ram_cancel:
            context->complete(epoc::error_none);
            return;

        case memory_manager_notify_low_ram:
            if (!low_ram_nof_.empty()) {
                context->complete(epoc::error_in_use);
                return;
            }

            low_ram_nof_ = epoc::notify_info(context->msg->request_sts, context->msg->own_thr);
            low_ram_requester_ = context->msg->own_thr->unique_id();
            return;

        case memory_manager_notify_low_ram_cancel:
            cancel_low_ram_notification();
            context->complete(epoc::error_none);
            return;

        default:
            break;
        }

        LOG_ERROR(SERVICE_UI, "Unimplemented memory manager opcode: {}", context->msg->function);
    }

    memory_manager_server::memory_manager_server(eka2l1::system *sys)
        : service::typical_server(sys, "MemoryManagerServer") {
    }

    void memory_manager_server::connect(service::ipc_context &context) {
        create_session<memory_manager_session>(&context);
        context.complete(epoc::error_none);
    }
}
