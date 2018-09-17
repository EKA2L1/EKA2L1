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

#include <atomic>
#include <clocale>
#include <memory>
#include <regex>
#include <unordered_map>

namespace eka2l1::epoc {
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

    struct io_component;
    using io_component_ptr = std::shared_ptr<io_component>;

    enum class fs_node_share {
        exclusive,
        share_read,
        share_read_write,
        any
    };

    struct fs_node {
        io_component_ptr vfs_node;
        uint32_t access_count;

        int mix_mode;
        int open_mode;

        fs_node_share share_mode;

        process_ptr own_process;

        bool is_active = false;

        uint32_t id;
    };

    enum {
        fs_max_handle = 0x200
    };

    class fs_handle_table {
        std::array<fs_node, fs_max_handle> nodes;

    public:
        fs_handle_table();

        size_t add_node(fs_node &node);
        bool close_nodes(size_t handle);

        fs_node *get_node(size_t handle);
        fs_node *get_node(const std::u16string &path);
    };

    struct fs_path_case_insensitive_hasher {
        size_t operator()(const utf16_str &key) const;
    };

    struct fs_path_case_insensitive_comparer {
        bool operator()(const utf16_str &x, const utf16_str &y) const;
    };

    class fs_server : public service::server {
        fs_handle_table nodes_table;

        void file_open(service::ipc_context ctx);
        void file_create(service::ipc_context ctx);
        void file_replace(service::ipc_context ctx);
        void file_flush(service::ipc_context ctx);
        void file_close(service::ipc_context ctx);

        void new_file_subsession(service::ipc_context ctx, bool overwrite = false);

        void entry(service::ipc_context ctx);

        void file_size(service::ipc_context ctx);
        void file_seek(service::ipc_context ctx);
        void file_read(service::ipc_context ctx);
        void file_write(service::ipc_context ctx);

        void file_rename(service::ipc_context ctx);

        void open_dir(service::ipc_context ctx);
        void read_dir_packed(service::ipc_context ctx);
        void read_dir(service::ipc_context ctx);
        void close_dir(service::ipc_context ctx);

        void drive_list(service::ipc_context ctx);
        void drive(service::ipc_context ctx);

        void is_file_in_rom(service::ipc_context ctx);

        void session_path(service::ipc_context ctx);
        void set_session_path(service::ipc_context ctx);
        void set_session_to_private(service::ipc_context ctx);

        void synchronize_driver(service::ipc_context ctx);
        void notify_change_ex(service::ipc_context ctx);

        void private_path(service::ipc_context ctx);
        void mkdir(service::ipc_context ctx);
        void rename(service::ipc_context ctx);
        void replace(service::ipc_context ctx);

        void delete_entry(service::ipc_context ctx);

        void connect(service::ipc_context ctx) override;

        std::unordered_map<uint32_t, fs_node> file_nodes;
        std::unordered_map<uint32_t, utf16_str> session_paths;

        enum class notify_type {
            entry = 1,
            all = 2,
            file = 4,
            dir = 8,
            attrib = 0x10,
            write = 0x20,
            disk = 0x40
        };

        struct notify_entry {
            std::regex match_pattern;
            notify_type type;
            epoc::request_status *request_status;
        };

        std::vector<notify_entry> notify_entries;

        void notify(const utf16_str &entry, const notify_type type);

        int new_node(io_system *io, thread_ptr sender, std::u16string name, int org_mode, bool overwrite = false);
        fs_node *get_file_node(int handle);

    public:
        fs_server(system *sys);
    };
}