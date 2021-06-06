/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <common/log.h>
#include <utils/err.h>

#include <kernel/kernel.h>
#include <kernel/server.h>
#include <kernel/timing.h>

#include <config/config.h>

namespace eka2l1::service {
    server::~server() {
    }

    // Create a server with name
    server::server(kernel_system *kern, system *sys, kernel::thread *owner, const std::string name, bool hle, bool unhandle_callback_enable)
        : kernel_obj(kern, name, nullptr, kernel::access_type::global_access)
        , sys(sys)
        , hle(hle)
        , owner_thread(owner)
        , unhandle_callback_enable(unhandle_callback_enable) {
        obj_type = kernel::object_type::server;

        if (owner_thread)
            owner_thread->increase_access_count();

        REGISTER_IPC(server, connect, -1, "Server::Connect");
        REGISTER_IPC(server, disconnect, -2, "Server::Disconnect");
    }

    void server::receive(ipc_msg_ptr &msg) {
        msg = nullptr;

        if (!delivered_msgs.empty()) {
            common::double_linked_queue_element *deliver_first = delivered_msgs.first();
            msg = E_LOFF(deliver_first, ipc_msg, delivered_msg_link);

            deliver_first->deque();
        }
    }

    bool server::ready() {
        return request_status && request_data;
    }

    int server::deliver(ipc_msg_ptr msg) {
        // Is ready
        if (ready()) {
            accept(msg, true);
        } else {
            msg->msg_status = ipc_message_status::delivered;
            delivered_msgs.push(&msg->delivered_msg_link);
        }

        return 0;
    }

    void server::register_ipc_func(uint32_t ordinal, ipc_func func) {
        ipc_funcs.emplace(ordinal, func);
    }

    void server::detach(session *svse) {
        auto ite = std::find(sessions.begin(), sessions.end(), svse);
        if (ite != sessions.end()) {
            sessions.erase(ite);
        }
    }

    void server::destroy() {
        if (owner_thread)
            owner_thread->decrease_access_count();

        for (std::size_t i = 0; i < sessions.size(); i++) {
            sessions[i]->detatch(epoc::error_server_terminated);

            if (!is_hle()) {
                // HLE may still need server pointer for deinit
                sessions[i]->svr = nullptr;
            }
        }

        sessions.clear();
    }

    void server::accept(ipc_msg_ptr msg, const bool notify_owner) {
        msg->msg_status = ipc_message_status::accepted;

        // On some platforms where IPCv1 is still relevant, odd pointer is used to indicates new IPCv2
        if (kern->is_ipc_old() || (kern->is_eka1() && !(request_data.ptr_address() & 1))) {
            message1 *dat_hle = request_data.cast<message1>().get(request_own_thread->owning_process());

            dat_hle->ipc_msg_handle = msg->id;
            dat_hle->function = msg->function;
            dat_hle->session_ptr = msg->session_ptr_lle;
            dat_hle->client_thread_handle = kern->open_handle_with_thread(request_own_thread,
                msg->own_thr, kernel::owner_type::thread);

            std::copy(msg->args.args, msg->args.args + 4, dat_hle->args);
            msg->thread_handle_low = dat_hle->client_thread_handle;
        } else {
            request_data = eka2l1::ptr<message2>(request_data.ptr_address() & ~1);
            message2 *dat_hle = request_data.get(request_own_thread->owning_process());

            dat_hle->ipc_msg_handle = msg->id;
            dat_hle->flags = msg->args.flag;
            dat_hle->function = msg->function;
            dat_hle->session_ptr = msg->session_ptr_lle;

            std::copy(msg->args.args, msg->args.args + 4, dat_hle->args);
        }

        (request_status.get(request_own_thread->owning_process()))->set(0, kern->is_eka1()); // KErrNone

        if (notify_owner) {
            request_own_thread->signal_request();
        }

        request_own_thread = nullptr;
        request_status = 0;
        request_data = 0;
    }

    void server::receive_async_lle(eka2l1::ptr<epoc::request_status> msg_request_status,
        eka2l1::ptr<message2> data) {
        ipc_msg_ptr pending_msg = nullptr;
        receive(pending_msg);

        request_status = msg_request_status;
        request_own_thread = kern->crr_thread();
        request_data = data;

        if (!pending_msg) {
            return;
        }

        LOG_TRACE(KERNEL, "At least one message is on queue, popping and finish receive request");
        accept(pending_msg, true);
    }

    void server::cancel_async_lle() {
        if (!request_status) {
            request_own_thread->signal_request();
            return;
        }

        (request_status.get(request_own_thread->owning_process()))->set(-3, kern->is_eka1()); // KErrCancel
        request_own_thread->signal_request();

        request_own_thread = nullptr;
        request_status = 0;
        request_data = 0;
    }

    // IPC process related functions belong in context.cpp
}
