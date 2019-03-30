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

#include <scripting/message.h>
#include <scripting/instance.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>

#include <pybind11/pybind11.h>

namespace eka2l1::scripting {
    ipc_message_wrapper::ipc_message_wrapper(std::uint64_t handle) 
        : msg_(reinterpret_cast<ipc_msg*>(handle)) {
    }

    int ipc_message_wrapper::function() {
        return msg_->function;
    }

    std::uint32_t ipc_message_wrapper::arg(const int idx) {
        if (idx < 0 || idx >= 4) {
            throw pybind11::index_error("Arg index must be between 0 and 3!");
            return 0;
        }

        return msg_->args.args[idx];
    }

    std::unique_ptr<scripting::thread> ipc_message_wrapper::sender() {
        return std::make_unique<scripting::thread>(reinterpret_cast<std::uint64_t>(msg_->own_thr));
    }
    
    std::unique_ptr<ipc_message_wrapper> message_from_handle(const int guest_handle) {
        ipc_msg_ptr msg = get_current_instance()->get_kernel_system()->get_msg(guest_handle);

        if (!msg) {
            return nullptr;
        }

        return std::make_unique<ipc_message_wrapper>(reinterpret_cast<std::uint64_t>(&(*msg)));
    }
}
