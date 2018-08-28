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

#include <core/kernel/kernel_obj.h>
#include <core/kernel/thread.h>
#include <core/kernel/wait_obj.h>

#include <core/ipc.h>

#include <functional>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace eka2l1 {
    using thread_ptr = std::shared_ptr<kernel::thread>;

    namespace service {
        class server;
    }

    using server_ptr = std::shared_ptr<service::server>;

    namespace service {
        /*! \brief An IPC session 
		 *
         *  A session is a bridge between server and client.
         *  We can send message to the server and receive result.
         *  Server can be in different process and different thread.
        */
        class session : public kernel::kernel_obj {
            server_ptr svr;

            std::vector<std::pair<bool, ipc_msg_ptr>> msgs_pool;
            uint32_t cookie_address;

        private:
            ipc_msg_ptr &get_free_msg();

        public:
            session(kernel_system *kern, server_ptr svr, int async_slot_count);

            void prepare_destroy();

            int send_receive_sync(int function, ipc_arg args, int *request_sts);
            int send_receive_sync(int function, ipc_arg args);
            int send_receive_sync(int function);
            int send_receive(int function, ipc_arg args, int *request_sts);
            int send_receive(int function, int *request_sts);
            int send(int function, ipc_arg args);
            int send(int function);

            void set_cookie_address(const uint32_t addr) {
                cookie_address = addr;
            }

            /*! Send and receive an message synchronously */
            int send_receive_sync(ipc_msg_ptr &msg);

            /*! Send and receive an message asynchronously */
            int send_receive(ipc_msg_ptr &msg);

            /*! Send a message blind. Meaning that we don't care about what it returns */
            int send(ipc_msg_ptr &msg);

            void set_slot_free(ipc_msg_ptr &msg);
        };
    }
}