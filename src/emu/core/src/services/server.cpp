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
#include <core/core.h>
#include <core/services/server.h>

namespace eka2l1 {
    namespace service {
        void server::connect(service::ipc_context ctx) {
            ctx.set_request_status(0);
        }

        void server::disconnect(service::ipc_context ctx) {
            ctx.set_request_status(0);
        }

        bool server::is_msg_delivered(ipc_msg_ptr &msg) {
            auto &res = std::find_if(delivered_msgs.begin(), delivered_msgs.end(),
                [&](const auto &svr_msg) { return svr_msg.real_msg->id == msg->id; });

            if (res != delivered_msgs.end()) {
                return true;
            }

            return false;
        }

        // Create a server with name
        server::server(system *sys, const std::string name, bool hle)
            : sys(sys)
            , hle(hle)
            , kernel_obj(sys->get_kernel_system(), name, kernel::access_type::global_access) {
            kernel_system *kern = sys->get_kernel_system();
            process_msg = kern->create_msg(kernel::owner_type::process);

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

            msg.real_msg->msg_session->set_slot_free(msg.real_msg);

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

        // Processed asynchronously, use for HLE service where accepted function
        // is fetched imm
        void server::process_accepted_msg() {
            int res = receive(process_msg);

            if (res == -1) {
                return;
            }

            int func = process_msg->function;

            auto func_ite = ipc_funcs.find(func);

            if (func_ite == ipc_funcs.end()) {
                LOG_INFO("Unimplemented IPC call: 0x{:x} for server: {}", func, obj_name);

                // Signal request semaphore, to tell everyone that it has finished random request
                process_msg->own_thr->signal_request();

                return;
            }

            ipc_func ipf = func_ite->second;
            ipc_context context{ sys, process_msg };

            if (sys->get_bool_config("log_ipc")) {
                LOG_INFO("Calling IPC: {}, id: {}", ipf.name, func);
            }

            ipf.wrapper(context);
        }

        void server::destroy() {
            sys->get_kernel_system()->free_msg(process_msg);
        }

        void server::finish_request_lle(ipc_msg_ptr &msg, bool notify_owner) {
            request_data->ipc_msg_handle = msg->id;
            request_data->flags = msg->args.flag;
            request_data->function = msg->function;
            request_data->session_ptr = msg->session_ptr_lle;

            std::copy(msg->args.args, msg->args.args + 4, request_data->args);

            *request_status = 0; // KErrNone

            if (notify_owner) {
                request_own_thread->signal_request();
            }

            request_data = nullptr;
            request_own_thread = nullptr;
            request_status = nullptr;
        }

        void server::receive_async_lle(epoc::request_status *msg_request_status, message2 *data) {
            ipc_msg_ptr msg = sys->get_kernel_system()->create_msg(kernel::owner_type::process);

            msg->free = false;

            int res = receive(msg);

            request_status = msg_request_status;

            request_own_thread = sys->get_kernel_system()->crr_thread();
            request_data = data;

            if (res == -1) {
                request_msg = msg;

                return;
            }

            finish_request_lle(msg, true);
        }

        void server::cancel_async_lle() {
            if (!request_status) {
                request_own_thread->signal_request();
                return;
            }

            *request_status = -3; // KErrCancel

            request_own_thread->signal_request();

            request_data = nullptr;
            request_own_thread = nullptr;
            request_status = nullptr;

            request_msg = nullptr;
        }
    }
}