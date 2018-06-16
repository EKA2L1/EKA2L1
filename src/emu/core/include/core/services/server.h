/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#pragma once

#include <kernel/kernel_obj.h>
#include <services/session.h>

#include <functional>
#include <queue>
#include <string>
#include <unordered_map>

namespace eka2l1 {
    namespace service {
        // Arguments: IPC message, which contains the context
        using ipc_func_wrapper = std::function<void(ipc_msg)>;
        using session_ptr = std::shared_ptr<session>;

        struct ipc_func {
            ipc_func_wrapper wrapper;
            std::string name;
        };

        struct server_msg {
            // The IPC context
            ipc_msg real_msg;

            // Mark if the message is requested to be replied asynchronously.
            bool async;

            // If async = false, this will be a nullptr, else, it will points to a TRequestStatus,
            // which basiclly is an int.
            eka2l1::ptr<int> request_status;
        };

        class server : public kernel::kernel_obj {
            std::queue<session> sessions;
            std::queue<server_msg> pending_msgs;

            std::unordered_map<uint32_t, ipc_func> ipc_funcs;

        public:
            /* 
               Send and receive synchronous.
            */
            int send_receive(ipc_msg &msg);

            /* 
               Send a message blind, Receive nothing.
            */
            int send(ipc_msg &msg);
        };
    }
}