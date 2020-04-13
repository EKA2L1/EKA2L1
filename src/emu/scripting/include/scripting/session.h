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

#include <memory>
#include <string>

#include <scripting/server.h>

namespace eka2l1::service {
    class session;
}

namespace eka2l1::scripting {
    class session_wrapper {
        service::session *ss_;

    public:
        explicit session_wrapper(std::uint64_t handle);
        std::unique_ptr<scripting::server_wrapper> server();
    };

    std::unique_ptr<session_wrapper> session_from_handle(const std::uint32_t handle);
}
