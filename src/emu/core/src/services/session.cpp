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

#include <core/services/server.h>
#include <core/services/session.h>

#include <core/core_kernel.h>

#include <common/log.h>

namespace eka2l1 {
    namespace service {
        session::session(kernel_system *kern, server_ptr svr, int async_slot_count)
            : svr(svr)
            , cookie_address(0)
            , kernel_obj(kern, "", kernel::access_type::global_access) {
            obj_type = kernel::object_type::session;

            svr->attach(this);

            if (async_slot_count > 0) {
                msgs_pool.resize(async_slot_count);

                for (auto &msg : msgs_pool) {
                    msg = std::make_pair(true, kern->create_msg(kernel::owner_type::process));
                }
            }
        }

        ipc_msg_ptr &session::get_free_msg() {
            if (msgs_pool.empty()) {
                return kern->create_msg(kernel::owner_type::process);
            }

            auto &free_msg_in_pool = std::find_if(msgs_pool.begin(), msgs_pool.end(),
                [](const auto &msg) { return msg.first; });

            if (free_msg_in_pool != msgs_pool.end()) {
                free_msg_in_pool->first = false;
                return free_msg_in_pool->second;
            }

            return ipc_msg_ptr(nullptr);
        }

        void session::set_slot_free(ipc_msg_ptr &msg) {
            if (msg->own_thr->get_sync_msg() == msg) {
                return;
            }

            if (msgs_pool.empty()) {
                msg->free = true;
                return;
            }

            auto &wrap_msg = std::find_if(msgs_pool.begin(), msgs_pool.end(),
                [&](const auto &wrap_msg) { return wrap_msg.second == msg; });

            if (wrap_msg != msgs_pool.end()) {
                wrap_msg->first = true;
            }
        }

        // This behaves a little different then other
        int session::send_receive_sync(int function, ipc_arg args, epoc::request_status *request_sts) {
            ipc_msg_ptr &msg = kern->crr_thread()->get_sync_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = args;
            msg->own_thr = kern->crr_thread();
            msg->request_sts = request_sts;

            send_receive(msg);

            LOG_TRACE("Sending to {}, function: 0x{:x}", this->svr->name(), msg->function);

            if (msg->function == -1) {
                struct version {
                    uint8_t major;
                    uint8_t minor;
                    uint16_t build;
                };

                version sreq_ver = *reinterpret_cast<version *>(&args.args[0]);
                LOG_TRACE("Requested server version: {}.{}.{}", sreq_ver.major, sreq_ver.minor, sreq_ver.build);
            }
            
            return 0;
        }

        int session::send_receive_sync(int function, ipc_arg args) {
            epoc::request_status local_response = 0x80000001;
            ipc_msg_ptr &msg = kern->crr_thread()->get_sync_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = args;
            msg->own_thr = kern->crr_thread();
            msg->request_sts = &local_response;

            return send_receive_sync(msg);
        }

        int session::send_receive_sync(int function) {
            epoc::request_status local_response = 0x80000001;

            ipc_msg_ptr &msg = kern->crr_thread()->get_sync_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = ipc_arg(0, 0, 0, 0, 0);
            msg->own_thr = kern->crr_thread();
            msg->request_sts = &local_response;

            return send_receive_sync(msg);
        }

        int session::send_receive(int function, ipc_arg args, epoc::request_status *request_sts) {
            ipc_msg_ptr msg = get_free_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = args;
            msg->request_sts = request_sts;
            msg->own_thr = kern->crr_thread();

            send_receive(msg);

            return 0;
        }

        int session::send_receive(int function, epoc::request_status *request_sts) {
            ipc_msg_ptr msg = get_free_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = ipc_arg(0, 0, 0, 0, 0);
            msg->request_sts = request_sts;

            return send_receive(msg);
        }

        int session::send(int function, ipc_arg args) {
            ipc_msg_ptr msg = get_free_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = args;

            msg->request_sts = nullptr;

            return send(msg);
        }

        int session::send(int function) {
            ipc_msg_ptr msg = get_free_msg();

            if (!msg) {
                return -1;
            }

            msg->function = function;
            msg->args = ipc_arg(0, 0, 0, 0, 0);

            msg->request_sts = nullptr;

            return send(msg);
        }

        int session::send_receive_sync(ipc_msg_ptr &msg) {
            server_msg smsg;
            smsg.real_msg = msg;
            smsg.real_msg->msg_status = ipc_message_status::delivered;
            smsg.real_msg->msg_session = std::dynamic_pointer_cast<service::session>(kern->get_kernel_obj_by_id(uid));
            smsg.real_msg->session_ptr_lle = cookie_address;

            int deliver_success
                = svr->deliver(smsg);

            svr->process_accepted_msg();

            return smsg.real_msg->request_sts->status;
        }

        int session::send_receive(ipc_msg_ptr &msg) {
            server_msg smsg;

            smsg.real_msg = msg;
            smsg.real_msg->msg_status = ipc_message_status::delivered;
            smsg.real_msg->msg_session = std::dynamic_pointer_cast<service::session>(kern->get_kernel_obj_by_id(uid));
            smsg.real_msg->session_ptr_lle = cookie_address;

            return svr->deliver(smsg);
        }

        // Send blind message
        int session::send(ipc_msg_ptr &msg) {
            server_msg smsg;

            smsg.real_msg = msg;
            smsg.real_msg->msg_status = ipc_message_status::delivered;
            smsg.real_msg->request_sts = nullptr;
            smsg.real_msg->msg_session = std::dynamic_pointer_cast<service::session>(kern->get_kernel_obj_by_id(uid));
            smsg.real_msg->session_ptr_lle = cookie_address;

            return svr->deliver(smsg);
        }

        void session::prepare_destroy() {
            for (const auto &msg : msgs_pool) {
                kern->free_msg(msg.second);
            }
        }
    }
}