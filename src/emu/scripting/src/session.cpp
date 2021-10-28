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

#include <scripting/instance.h>
#include <scripting/session.h>

#include <kernel/kernel.h>
#include <kernel/session.h>
#include <system/epoc.h>

namespace eka2l1::scripting {
    session_wrapper::session_wrapper(std::uint64_t handle)
        : ss_(reinterpret_cast<service::session *>(handle)) {
    }
}

extern "C" {
EKA2L1_EXPORT void eka2l1_free_session(eka2l1::scripting::session_wrapper *ss) {
    delete ss;
}

EKA2L1_EXPORT eka2l1::scripting::session_wrapper *eka2l1_session_from_handle(const std::uint32_t handle) {
    eka2l1::kernel_system *kern = eka2l1::scripting::get_current_instance()->get_kernel_system();
    eka2l1::service::session *ss = kern->get<eka2l1::service::session>(handle);

    if (!ss) {
        return nullptr;
    }

    return new eka2l1::scripting::session_wrapper(reinterpret_cast<std::uint64_t>(ss));
}

EKA2L1_EXPORT eka2l1::scripting::server_wrapper *eka2l1_session_server(eka2l1::scripting::session_wrapper *ss) {
    eka2l1::service::session *ss_impl = ss->get_session_handle();
    eka2l1::service::server *sv = ss_impl->get_server();

    if (!sv) {
        return nullptr;
    }

    return new eka2l1::scripting::server_wrapper(reinterpret_cast<std::uint64_t>(sv));
}
}