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
#include <kernel/thread.h>
#include <kernel/wait_obj.h>

#include <ipc.h>

#include <functional>
#include <map>
#include <memory>
#include <unordered_map>

namespace eka2l1 {
    using thread_ptr = std::shared_ptr<kernel::thread>;

    namespace service {
        class server; // It's a reference object anyway

        using server_ptr = std::shared_ptr<sever>;

        class session {
            server_ptr svr;

        public:
            int send_receive_sync(ipc_msg_ptr &msg);
            int send_receive(ipc_msg_ptr &msg, int &status);

            // Send blind message
            int send(ipc_msg_ptr &msg);
        };
    }
}