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
#include <kernel/session.h>

#include <utils/reqsts.h>

#include <functional>
#include <queue>
#include <string>
#include <unordered_map>

#include <memory>

#define REGISTER_IPC(server, func, op, func_name) \
    register_ipc_func(op,                         \
        service::ipc_func(func_name, std::bind(&server::func, this, std::placeholders::_1)));

namespace eka2l1 {
    class system;

    using session_ptr = service::session *;

    /*! \brief IPC implementation. */
    namespace service {
        struct server_msg;
        struct ipc_context;

        using ipc_func_wrapper = std::function<void(ipc_context &)>;
        using ipc_msg_ptr = ipc_msg *;

        using uid = std::uint32_t;

        /*! \brief A class represents an IPC function */
        struct ipc_func {
            ipc_func_wrapper wrapper;
            std::string name;

            ipc_func(std::string iname, ipc_func_wrapper wr)
                : name(std::move(iname))
                , wrapper(wr) {}
        };

        struct message1 {
            std::int32_t function;
            std::uint32_t args[4];
            std::uint32_t client_thread_handle;
            std::int32_t session_ptr;
            std::int32_t ipc_msg_handle;
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
        protected:
            system *sys;

        private:
            /** All the sessions connected to this server */
            std::vector<session *> sessions;
            common::roundabout delivered_msgs;

            /** The thread own this server */
            //thread_ptr owning_thread;

            /** Placeholder message uses for processing */
        protected:
            std::unordered_map<int, ipc_func> ipc_funcs;

        private:
            eka2l1::ptr<epoc::request_status> request_status = 0;
            eka2l1::ptr<message2> request_data;

            kernel::thread *owner_thread;
            kernel::thread *request_own_thread;

            bool hle = false;
            bool unhandle_callback_enable = false;

            service::share_mode shmode_;

        protected:
            bool ready();

            // These provides version in order to connect to the server
            // Security layer is ignored rn.
            //
            // Servers can override this method
            virtual void connect(service::ipc_context &ctx);
            virtual void disconnect(service::ipc_context &ctx);

            virtual void on_unhandled_opcode(service::ipc_context &ctx) {}
            void accept(ipc_msg_ptr msg, const bool notify_owner);

        public:
            std::uint32_t frequent_process_event;

            explicit server(kernel_system *kern, system *sys, kernel::thread *owner, const std::string name, bool hle = false,
                bool unhandle_callback_enable = false, const service::share_mode shmode = service::SHARE_MODE_GLOBAL_SHAREABLE);
            ~server() override;

            void attach(session *svse) {
                sessions.push_back(svse);
            }

            void detach(session *svse);

            virtual void destroy() override;

            int deliver(ipc_msg_ptr msg);
            void receive(ipc_msg_ptr &msg);

            void receive_async_lle(eka2l1::ptr<epoc::request_status> request_status,
                eka2l1::ptr<message2> data);
            void cancel_async_lle();

            void register_ipc_func(uint32_t ordinal, ipc_func func);

            virtual void process_accepted_msg();

            virtual service::uid get_owner_secure_uid() const {
                return 0xDEADC11E;
            }

            virtual service::uid get_owner_vendor_uid() const {
                return 0;
            }

            system *get_system() {
                return sys;
            }

            bool is_hle() const {
                return hle;
            }

            kernel::thread *get_owner_thread() {
                return owner_thread;
            }

            service::share_mode get_share_mode() const {
                return shmode_;
            }
        };
    }
}
