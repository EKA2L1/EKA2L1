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

#include <services/context.h>
#include <services/server.h>

namespace epoc {
    struct TTime {
        uint64_t aTimeEncoded;
    };

    struct TEntry {
        uint32_t aAttrib;
        uint32_t aSize;
        
        TTime aModified;
        uint32_t uid1;
        uint32_t uid2;
        uint32_t uid3;

        uint32_t aNameLength;
        uint16_t aName[0x100];

        uint32_t aSizeHigh;
        uint32_t aReversed;
    };

}

namespace eka2l1 {
    class fs_server : public service::server {
        void file_open(service::ipc_context ctx);
        void file_create(service::ipc_context ctx);
        void file_replace(service::ipc_context ctx);

        void entry(service::ipc_context ctx);

    public:
        fs_server(system *sys);
    };
}