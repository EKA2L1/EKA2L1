/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <scripting/instance.h>
#include <scripting/message.h>

#include <system/epoc.h>
#include <kernel/kernel.h>

#if ENABLE_PYTHON_SCRIPTING
#include <pybind11/pybind11.h>
#endif

namespace eka2l1::scripting {
    ipc_message_wrapper::ipc_message_wrapper(std::uint64_t handle)
        : msg_(reinterpret_cast<ipc_msg *>(handle)) {
    }

    int ipc_message_wrapper::function() {
        return msg_->function;
    }

    std::uint32_t ipc_message_wrapper::arg(const int idx) {
        if (idx < 0 || idx >= 4) {
#if ENABLE_PYTHON_SCRIPTING
            throw pybind11::index_error("Arg index must be between 0 and 3!");
#endif
            return 0;
        }

        return msg_->args.args[idx];
    }

    std::uint32_t ipc_message_wrapper::flags() const {
        return static_cast<std::uint32_t>(msg_->args.flag);
    }

    std::uint32_t ipc_message_wrapper::request_status_address() const {
        return msg_->request_sts.ptr_address();
    }

    std::unique_ptr<scripting::thread> ipc_message_wrapper::sender() {
        return std::make_unique<scripting::thread>(reinterpret_cast<std::uint64_t>(msg_->own_thr));
    }

    std::unique_ptr<scripting::session_wrapper> ipc_message_wrapper::session() {
        return std::make_unique<scripting::session_wrapper>(reinterpret_cast<std::uint64_t>(msg_->msg_session));
    }

    std::unique_ptr<ipc_message_wrapper> message_from_handle(const int guest_handle) {
        ipc_msg_ptr msg = get_current_instance()->get_kernel_system()->get_msg(guest_handle);

        if (!msg) {
            return nullptr;
        }

        return std::make_unique<ipc_message_wrapper>(reinterpret_cast<std::uint64_t>(msg.get()));
    }
}

extern "C" {
	EKA2L1_EXPORT void symemu_free_ipc_msg(eka2l1::scripting::ipc_message_wrapper *msg) {
        delete msg;
    }

    EKA2L1_EXPORT eka2l1::scripting::ipc_message_wrapper *symemu_ipc_message_from_handle(const int guest_handle) {
        eka2l1::ipc_msg_ptr msg = eka2l1::scripting::get_current_instance()->get_kernel_system()->get_msg(guest_handle);

        if (!msg) {
            return nullptr;
        }

        return new eka2l1::scripting::ipc_message_wrapper(reinterpret_cast<std::uint64_t>(msg.get()));
    }

    EKA2L1_EXPORT int symemu_ipc_message_function(eka2l1::scripting::ipc_message_wrapper *msg) {
        return msg->function();
    }

    EKA2L1_EXPORT std::uint32_t symemu_ipc_message_arg(eka2l1::scripting::ipc_message_wrapper *msg, const int idx) {
        return msg->arg(idx);
    }

    EKA2L1_EXPORT std::uint32_t symemu_ipc_message_flags(eka2l1::scripting::ipc_message_wrapper *msg) {
        return msg->flags();
    }

    EKA2L1_EXPORT std::uint32_t symemu_ipc_message_request_status_address(eka2l1::scripting::ipc_message_wrapper *msg) {
        return msg->request_status_address();
    }

    EKA2L1_EXPORT eka2l1::scripting::thread *symemu_ipc_message_sender(eka2l1::scripting::ipc_message_wrapper *msg) {
        eka2l1::kernel::thread *thh = msg->get_message_handler()->own_thr;
        return new eka2l1::scripting::thread(reinterpret_cast<std::uint64_t>(thh));
    }

    EKA2L1_EXPORT eka2l1::scripting::session_wrapper *symemu_ipc_message_session_wrapper(eka2l1::scripting::ipc_message_wrapper *msg) {
        eka2l1::service::session *ss = msg->get_message_handler()->msg_session;
        return new eka2l1::scripting::session_wrapper(reinterpret_cast<std::uint64_t>(ss));
    }
}