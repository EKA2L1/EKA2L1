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

#include <core/services/context.h>
#include <core/services/server.h>

#include <core/ptr.h>

namespace eka2l1 {
    struct posix_params {
        int ret;
        int fid;
        int pint[3];
        eka2l1::ptr<char> cptr[2];
        eka2l1::ptr<char> ptr[2];
        eka2l1::ptr<std::int16_t> cwptr[2];
        eka2l1::ptr<std::int16_t> wptr[2];
        eka2l1::ptr<eka2l1::ptr<std::int16_t>> eptr[1];
        unsigned long len[1];
        eka2l1::ptr<unsigned long> lenp[1];
        int addr;
    };

    class posix_file_manager {
        
    };

    class posix_server : public service::server {
        process_ptr associated_process;
        std::u16string working_dir;

        void open(service::ipc_context ctx);
        void close(service::ipc_context ctx);
        void lseek(service::ipc_context ctx);
        void fstat(service::ipc_context ctx);

        /*!\brief Change the current directory of the server. */
        void chdir(service::ipc_context ctx);
        void mkdir(service::ipc_context ctx);

    public:
        posix_server(eka2l1::system *sys, process_ptr &associated_process);
    };
}