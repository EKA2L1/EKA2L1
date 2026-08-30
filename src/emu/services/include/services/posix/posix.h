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

#include <kernel/server.h>
#include <services/context.h>

#include <mem/ptr.h>
#include <vfs/vfs.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace eka2l1 {
    struct posix_params {
        std::int32_t ret;
        std::int32_t fid;
        std::int32_t pint[3];
        eka2l1::ptr<char> cptr[2];
        eka2l1::ptr<char> ptr[2];
        eka2l1::ptr<std::int16_t> cwptr[2];
        eka2l1::ptr<std::int16_t> wptr[2];
        eka2l1::ptr<eka2l1::ptr<std::int16_t>> eptr[1];
        std::uint32_t len[1];
        eka2l1::ptr<std::uint32_t> lenp[1];
        std::int32_t addr;
    };

    static_assert(sizeof(posix_params) == 68, "The POSIX IPC block uses the 32-bit Symbian ABI");

    // S60 2nd Edition's libc/sys/stat.h layout. Do not use the host's struct stat:
    // its field widths and alignment differ on every 64-bit host.
    struct posix_stat {
        std::int16_t device;
        std::uint16_t inode;
        std::int32_t mode;
        std::int16_t link_count;
        std::uint16_t user_id;
        std::uint16_t group_id;
        std::int16_t special_device;
        std::int32_t size;
        std::int32_t access_time;
        std::int32_t spare1;
        std::int32_t modification_time;
        std::int32_t spare2;
        std::int32_t change_time;
        std::int32_t spare3;
        std::int32_t block_size;
        std::int32_t blocks;
        std::int32_t spare4[2];
    };

    static_assert(sizeof(posix_stat) == 60, "Unexpected S60v2 stat layout");

    using fid = int;

    class posix_file_manager {
        struct open_file_description {
            std::shared_ptr<file> handle;
            bool readable;
            bool writable;
        };

        enum {
            MAX_FID = 32768
        };

        // POSIX duplicates share one open-file description, including its seek
        // position. The underlying VFS file closes with the last descriptor.
        std::vector<std::shared_ptr<open_file_description>> files;
        io_system *io;

    protected:
        fid get_lowest_usable_fid();

    public:
        explicit posix_file_manager(io_system *io);

        fid open(const std::u16string &path, int mode, bool readable, bool writable, int &terrno);
        void close(const fid id, int &terrno);
        size_t seek(const fid id, const int off, const eka2l1::file_seek_mode whine, int &terrno);

        std::optional<fid> duplicate(const fid id, int &terrno);
        int duplicate_provide_fid(const fid newid, const fid oldid);

        size_t tell(const fid id, int &terrno);
        size_t read(const fid id, const size_t len, char *buf, int &terrno);
        size_t write(const fid id, const size_t len, char *buf, int &terrno);

        void stat(const fid id, posix_stat *filestat, int &terrno);
    };

    class posix_server : public service::server {
        std::uint32_t associated_process_uid;
        posix_file_manager file_manager;

        std::u16string working_dir;

        void open(service::ipc_context &ctx);
        void close(service::ipc_context &ctx);
        void lseek(service::ipc_context &ctx);
        void fstat(service::ipc_context &ctx);

        void read(service::ipc_context &ctx);
        void write(service::ipc_context &ctx);

        /*!\brief Change the current directory of the server. */
        void chdir(service::ipc_context &ctx);
        void mkdir(service::ipc_context &ctx);
        void unlink(service::ipc_context &ctx);

        void dup(service::ipc_context &ctx);
        void dup2(service::ipc_context &ctx);

    public:
        posix_server(eka2l1::system *sys, process_ptr &associated_process,
            const std::u16string &executable_path);
        void update_executable_path(const std::u16string &executable_path);
    };
}
