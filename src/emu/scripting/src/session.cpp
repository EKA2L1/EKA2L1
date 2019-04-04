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

#include <scripting/session.h>
#include <scripting/instance.h>

#include <epoc/services/session.h>
#include <epoc/epoc.h>
#include <epoc/kernel.h>

namespace eka2l1::scripting {
    session_wrapper::session_wrapper(std::uint64_t handle) 
        : ss_(reinterpret_cast<service::session*>(handle)) {
    }
    
    std::unique_ptr<scripting::server_wrapper> session_wrapper::server() {
        return std::make_unique<scripting::server_wrapper>(
            reinterpret_cast<std::uint64_t>(&(*ss_->get_server())));
    }
    
    std::unique_ptr<session_wrapper> session_from_handle(const std::uint32_t handle) {
        return std::make_unique<scripting::session_wrapper>(reinterpret_cast<std::uint64_t>(&(
            *get_current_instance()->get_kernel_system()->get<service::session>(handle))));
    }
}
