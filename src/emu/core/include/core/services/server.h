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
#include <services/context.h>

#include <functional>
#include <queue>
#include <string>
#include <unordered_map>

namespace eka2l1 {
    class system;

    namespace service {
        struct server_msg;

        // Arguments: IPC message, which contains the context
        using ipc_func_wrapper = std::function<void(ipc_context)>;
        using session_ptr = std::shared_ptr<session>;
        using ipc_msg_ptr = std::shared_ptr<ipc_msg>;

        struct ipc_func {
            ipc_func_wrapper wrapper;
            std::string name;
        };

        enum class ipc_message_status {
            delivered,
            accepted,
            completed
        };

        struct server_msg {
            // The IPC context
            ipc_msg_ptr real_msg;

            int *request_status;

            // Status of the message, if it's accepted or delivered
            ipc_message_status msg_status;
        };

        /* A IPC HLE server. Supervisor thread will check the server and reply pending messages. */
        class server : public kernel::wait_obj {
            std::queue<session> sessions;
            std::vector<server_msg> delivered_msgs;

            std::vector<server_msg> accepted_msgs;
            std::vector<server_msg> completed_msgs;

            std::unordered_map<uint32_t, ipc_func> ipc_funcs;

            thread_ptr owning_thread;
            system *sys;

        protected:
            bool is_msg_delivered(ipc_msg_ptr &msg);

        public:
            int receive(ipc_msg_ptr &msg, int &request_sts);
            int accept(server_msg msg);
            int deliver(server_msg msg);
            int cancel();

            void register_ipc_func(uint32_t ordinal, ipc_func func);

            // Processed asynchronously
            void process_accepted_msg();
        };
    }
}