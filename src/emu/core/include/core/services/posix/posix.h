/*
* Copyright (c) 2018 EKA2L1 Team
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

#include <core/services/server.h>
#include <core/services/context.h>

namespace eka2l1 {
    class posix_server : public service::server {
        process_ptr associated_process;

        void open(service::ipc_context ctx);
        void close(service::ipc_context ctx);
        void lseek(service::ipc_context ctx);
        void fstat(service::ipc_context ctx);

    public:
        posix_server(eka2l1::system *sys, process_ptr &associated_process);
    };
}