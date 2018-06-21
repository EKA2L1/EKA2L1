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
#include <services/context.h>
#include <services/session.h>

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
                : name(std::move(iname)), wrapper(wr) {}
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

            std::unordered_map<uint32_t, ipc_func> ipc_funcs;

            /** The thread own this server */
            thread_ptr owning_thread;
            system *sys;

            /** Placeholder message uses for processing */
            ipc_msg_ptr process_msg;

        protected:
            bool is_msg_delivered(ipc_msg_ptr &msg);

        public:
            server(system *sys, const std::string name);

            void attach(session *svse) {
                sessions.push_back(svse);
            }

            void destroy();

            /*! Receive the message */
            int receive(ipc_msg_ptr &msg);

            /*! Accept a message that was waiting in the delivered queue */
            int accept(server_msg msg);

            /*! Deliver the message to the server. Message will be put in queue if it's not ready. */
            int deliver(server_msg msg);

            /*! Cancel a message in the delivered queue */
            int cancel();

            void register_ipc_func(uint32_t ordinal, ipc_func func);

            /*! Process an message asynchrounously */
            void process_accepted_msg();
        };
    }
}