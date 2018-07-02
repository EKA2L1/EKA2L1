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

#include <handle_table.h>

#include <memory>
#include <unordered_map>

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
    class io_system;

    struct file;
    using symfile = std::shared_ptr<file>;

    enum class fs_node_share {
        exclusive,
        share_read,
        share_read_write,
        any
    };

    struct fs_node {
        symfile vfs_node;
        uint32_t access_count;

        int mix_mode;
        int open_mode;

        fs_node_share share_mode;

        int id;
    };

    class fs_server : public service::server {
        void file_open(service::ipc_context ctx);
        void file_create(service::ipc_context ctx);
        void file_replace(service::ipc_context ctx);

        void entry(service::ipc_context ctx);

        void file_size(service::ipc_context ctx);

        handle_table<512> file_handles;
        std::unordered_map<uint32_t, fs_node> file_nodes;

        int new_node(io_system *io, std::u16string name, int org_mode);
        fs_node *get_file_node(int handle);

    public:
        fs_server(system *sys);
    };
}