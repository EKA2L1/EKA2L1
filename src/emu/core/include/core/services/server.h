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
#include <core/services/context.h>
#include <core/services/session.h>

#include <functional>
#include <queue>
#include <string>
#include <unordered_map>

#include <memory>

#define REGISTER_IPC(server, func, op, func_name) \
    register_ipc_func(op,                         \
        service::ipc_func(func_name, std::bind(&##server::##func, this, std::placeholders::_1)));

namespace eka2l1 {
    class system;

    using session_ptr = std::shared_ptr<service::session>;

    /*! \brief IPC implementation. */
    namespace service {
        struct server_msg;

        using ipc_func_wrapper = std::function<void(ipc_context)>;
        using ipc_msg_ptr = std::shared_ptr<ipc_msg>;

        /*! \brief A class represents an IPC function */
        struct ipc_func {
            ipc_func_wrapper wrapper;
            std::string name;

            ipc_func(std::string iname, ipc_func_wrapper wr)
                : name(std::move(iname))
                , wrapper(wr) {}
        };

        /*! \brief A class represents server message. 
         *
		 *  A server message is ready when it has the destination to send
        */
        struct server_msg {
            ipc_msg_ptr real_msg;
            ipc_msg_ptr dest_msg;

            bool is_ready() const {
                return false;
            }
        };

        struct message2 {
            int ipc_msg_handle;
            int function;
            int args[4];
            int spare1;
            int session_ptr;
            int flags;
            int spare3;
        };

        /*! \brief An IPC HLE server.
         * 
         *  The server can receive an message or receive them whenever they want.
         *  After messages were received, they will be processed by the fake HLE server and signal request semaphore of the client thread.
        */
        class server : public kernel::kernel_obj {
            /** All the sessions connected to this server */
            std::vector<session *> sessions;

            /** Messages that has been delivered but not accepted yet */
            std::vector<server_msg> delivered_msgs;

            std::unordered_map<int, ipc_func> ipc_funcs;

            system *sys;

            /** The thread own this server */
            thread_ptr owning_thread;

            /** Placeholder message uses for processing */
            ipc_msg_ptr process_msg;

            // These provides version in order to connect to the server
            // Security layer is ignored rn.
            void connect(service::ipc_context ctx);
            void disconnect(service::ipc_context ctx);

            int *request_status = nullptr;
            message2 *request_data = nullptr;

            thread_ptr request_own_thread;
            ipc_msg_ptr request_msg;

            void finish_request_lle(ipc_msg_ptr &session_msg, bool notify_owner);

            bool hle = false;

        protected:
            bool is_msg_delivered(ipc_msg_ptr &msg);
            bool ready();

        public:
            server(system *sys, const std::string name, bool hle = false);

            void attach(session *svse) {
                sessions.push_back(svse);
            }

            virtual void destroy();

            /*! Receive the message */
            int receive(ipc_msg_ptr &msg);

            /*! Accept a message that was waiting in the delivered queue */
            int accept(server_msg msg);

            /*! Deliver the message to the server. Message will be put in queue if it's not ready. */
            int deliver(server_msg msg);

            /*! Cancel a message in the delivered queue */
            int cancel();

            void receive_async_lle(int *request_status, message2 *data);

            void cancel_async_lle();

            void register_ipc_func(uint32_t ordinal, ipc_func func);

            /*! Process an message asynchrounously */
            void process_accepted_msg();

            bool is_hle() const {
                return hle;
            }
        };
    }
}