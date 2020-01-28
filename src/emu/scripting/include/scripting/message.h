/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <epoc/ipc.h>
#include <scripting/thread.h>
#include <scripting/server.h>
#include <scripting/session.h>

#include <cstddef>
#include <memory>

namespace eka2l1::scripting {
    class ipc_message_wrapper {
        eka2l1::ipc_msg *msg_;

    public:
        explicit ipc_message_wrapper(std::uint64_t handle);

        int function();
        std::unique_ptr<scripting::thread> sender();

        std::uint32_t arg(const int idx);
        std::uint32_t flags() const;
        
        std::unique_ptr<scripting::session_wrapper> session();
    };

    std::unique_ptr<ipc_message_wrapper> message_from_handle(const int guest_handle);
}
