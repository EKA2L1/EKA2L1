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

#include <kernel/kernel.h>
#include <kernel/server.h>
#include <kernel/timing.h>

#include <config/config.h>

namespace eka2l1 {
    namespace service {
        bool server::is_msg_delivered(ipc_msg_ptr &msg) {
            auto res = std::find_if(delivered_msgs.begin(), delivered_msgs.end(),
                [&](const auto &svr_msg) { return svr_msg.real_msg->id == msg->id; });

            if (res != delivered_msgs.end()) {
                return true;
            }

            return false;
        }

        server::~server() {
        }

        // Create a server with name
        server::server(kernel_system *kern, system *sys, const std::string name, bool hle, bool unhandle_callback_enable)
            : kernel_obj(kern, name, nullptr, kernel::access_type::global_access)
            , sys(sys)
            , hle(hle)
            , unhandle_callback_enable(unhandle_callback_enable) {
            process_msg = kern->create_msg(kernel::owner_type::process);
            process_msg->lock_free();

            obj_type = kernel::object_type::server;

            REGISTER_IPC(server, connect, -1, "Server::Connect");
            REGISTER_IPC(server, disconnect, -2, "Server::Disconnect");
        }

        int server::receive(ipc_msg_ptr &msg) {
            /* If there is pending message, pop the last one and accept it */
            if (!delivered_msgs.empty()) {
                server_msg yet_pending = std::move(delivered_msgs.back());

                yet_pending.dest_msg = msg;

                accept(yet_pending);
                delivered_msgs.pop_back();

                return 0;
            }

            return -1;
        }

        int server::accept(server_msg msg) {
            // Server is dead, nope, i wont accept you, never, or maybe ...
            // (bentokun) This may need in the future.
            /* if (owning_thread->current_state() == kernel::thread_state::stop) {
                return -1;
            } */

            msg.dest_msg->msg_status = ipc_message_status::accepted;
            msg.real_msg->msg_status = ipc_message_status::accepted;

            msg.dest_msg->args = msg.real_msg->args;
            msg.dest_msg->function = msg.real_msg->function;
            msg.dest_msg->request_sts = msg.real_msg->request_sts;
            msg.dest_msg->own_thr = msg.real_msg->own_thr;
            msg.dest_msg->session_ptr_lle = msg.real_msg->session_ptr_lle;
            msg.dest_msg->msg_session = msg.real_msg->msg_session;

            if (msg.real_msg->msg_session) {
                msg.real_msg->msg_session->set_slot_free(msg.real_msg);
            } else {
                kern->free_msg(msg.real_msg);
            }

            return 0;
        }

        bool server::ready() {
            return request_status && request_data;
        }

        int server::deliver(server_msg msg) {
            // Is ready
            if (ready()) {
                msg.dest_msg = request_msg;
                accept(msg);

                finish_request_lle(msg.dest_msg, true);
            } else {
                delivered_msgs.push_back(msg);
            }

            return 0;
        }

        int server::cancel() {
            delivered_msgs.pop_back();

            return 0;
        }

        void server::register_ipc_func(uint32_t ordinal, ipc_func func) {
            ipc_funcs.emplace(ordinal, func);
        }

        void server::destroy() {
            process_msg->unlock_free();
            kern->free_msg(process_msg);
        }

        void server::finish_request_lle(ipc_msg_ptr &msg, bool notify_owner) {
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
            ipc_msg_ptr msg = kern->create_msg(kernel::owner_type::process);
            msg->free = false;

            int res = receive(msg);

            request_status = msg_request_status;
            request_own_thread = kern->crr_thread();
            request_data = data;

            if (res == -1) {
                request_msg = msg;
                return;
            }

            LOG_TRACE(KERNEL, "At least one message is on queue, popping and finish receive request");
            finish_request_lle(msg, true);
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

            request_msg = nullptr;
        }

        // IPC process related functions belong in context.cpp
    }
}
