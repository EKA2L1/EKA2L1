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

#include <services/posix/op.h>
#include <services/posix/posix.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/platform.h>
#include <common/time.h>

#include <kernel/kernel.h>
#include <system/epoc.h>

#include <utils/err.h>

#include <algorithm>
#include <cerrno>
#include <limits>

#if EKA2L1_PLATFORM(POSIX)
#include <unistd.h>
#endif

#define POSIX_REQUEST_FINISH_WITH_ERR(ctx, err) \
    *errnoptr = err;                            \
    ctx.complete(epoc::error_none);             \
    return

#define POSIX_REQUEST_FINISH(ctx)   \
    ctx.complete(epoc::error_none); \
    return

#define POSIX_REQUEST_INIT(ctx)                                                             \
    kernel::process *own_process = ctx.msg->own_thr->owning_process();                      \
    const address errno_address = ctx.msg->args.args[0];                                    \
    const address params_address = ctx.msg->args.args[1];                                   \
    if (!valid_guest_range(own_process, errno_address, sizeof(std::int32_t))                \
        || !valid_guest_range(own_process, params_address, sizeof(eka2l1::posix_params))) { \
        ctx.complete(epoc::error_argument);                                                 \
        return;                                                                             \
    }                                                                                       \
    auto *errnoptr = eka2l1::ptr<std::int32_t>(errno_address).get(own_process);             \
    auto *params = eka2l1::ptr<eka2l1::posix_params>(params_address).get(own_process);      \
    *errnoptr = 0

namespace eka2l1 {
    namespace {
        constexpr std::size_t POSIX_MAX_PATH = 256;
        constexpr std::int32_t POSIX_S_IFREG = 0100000;
        constexpr std::int32_t POSIX_BLOCK_SIZE = 1024;

        // S60 2nd Edition's libc/sys/fcntl.h values. Host O_* constants are an
        // ABI detail of the build machine and cannot be used for guest IPC.
        constexpr std::int32_t POSIX_O_ACCMODE = 0x0003;
        constexpr std::int32_t POSIX_O_RDONLY = 0x0000;
        constexpr std::int32_t POSIX_O_WRONLY = 0x0001;
        constexpr std::int32_t POSIX_O_RDWR = 0x0002;
        constexpr std::int32_t POSIX_O_APPEND = 0x0008;
        constexpr std::int32_t POSIX_O_CREAT = 0x0200;
        constexpr std::int32_t POSIX_O_TRUNC = 0x0400;
        constexpr std::int32_t POSIX_O_EXCL = 0x0800;
        constexpr std::int32_t POSIX_O_TMPFILE = 0x10000000;

        bool valid_guest_range(kernel::process *process, const address start, const std::size_t size) {
            if (size == 0) {
                return true;
            }

            if ((start == 0) || (size - 1 > std::numeric_limits<address>::max() - start)) {
                return false;
            }

            auto *first = reinterpret_cast<std::uint8_t *>(process->get_ptr_on_addr_space(start));
            auto *last = reinterpret_cast<std::uint8_t *>(process->get_ptr_on_addr_space(
                start + static_cast<address>(size - 1)));
            return first && last && (last == first + size - 1);
        }

        std::optional<std::u16string> read_guest_path(kernel::process *process, const ptr<std::int16_t> path) {
            std::u16string result;
            result.reserve(POSIX_MAX_PATH);

            for (std::size_t i = 0; i < POSIX_MAX_PATH; ++i) {
                const address char_address = path.ptr_address() + static_cast<address>(i * sizeof(char16_t));
                auto *character = reinterpret_cast<char16_t *>(process->get_ptr_on_addr_space(char_address));
                if (!character) {
                    return std::nullopt;
                }

                if (*character == 0) {
                    return result;
                }

                result.push_back(*character);
            }

            return std::nullopt;
        }
    }

    posix_file_manager::posix_file_manager(io_system *io)
        : io(io) {}

    fid posix_file_manager::get_lowest_usable_fid() {
        if (files.size() == MAX_FID) {
            return 0;
        }

        const auto &free_slot = std::find_if_not(files.begin(), files.end(),
            [](const std::shared_ptr<open_file_description> &description) -> bool {
                return description.get();
            });

        if (free_slot == files.end()) {
            files.push_back(nullptr);
            return static_cast<fid>(files.size());
        }

        return static_cast<fid>(std::distance(files.begin(), free_slot)) + 1;
    }

    fid posix_file_manager::open(const std::u16string &path, const int mode,
        const bool readable, const bool writable, int &terrno) {
        const fid suit_fid = get_lowest_usable_fid();

        if (!suit_fid) {
            terrno = EMFILE;
            return 0;
        }

        symfile handle = io->open_file(path, mode);
        if (!handle) {
            terrno = ENOENT;
            return 0;
        }

        files[suit_fid - 1] = std::make_shared<open_file_description>(
            open_file_description{ std::move(handle), readable, writable });

        LOG_TRACE(SERVICE_POSIX, "File opened {}", common::ucs2_to_utf8(path));

        terrno = 0;
        return suit_fid;
    }

    void posix_file_manager::close(const fid id, int &terrno) {
        if (id > files.size() || !id || !files[id - 1]) {
            terrno = EBADF;
            return;
        }

        const bool success = (files[id - 1].use_count() > 1) || files[id - 1]->handle->close();

        if (!success) {
            terrno = EIO;
            return;
        }

        terrno = 0;
        files[id - 1] = nullptr;
    }

    std::optional<fid> posix_file_manager::duplicate(const fid id, int &terrno) {
        if (id > files.size() || !id || !files[id - 1]) {
            terrno = EBADF;
            return std::optional<fid>{};
        }

        const fid suit = get_lowest_usable_fid();

        if (!suit) {
            terrno = EMFILE;
            return std::optional<fid>{};
        }

        files[suit - 1] = files[id - 1];
        terrno = 0;
        return suit;
    }

    int posix_file_manager::duplicate_provide_fid(const fid newid, const fid oldid) {
        if (oldid > files.size() || !oldid || !files[oldid - 1]) {
            return EBADF;
        }

        if (newid == oldid) {
            return 0;
        }

        if ((newid > MAX_FID) || !newid) {
            return EBADF;
        }

        if (newid > files.size()) {
            files.resize(newid);
        }

        if (files[newid - 1]) {
            int err = 0;
            this->close(newid, err);

            if (err) {
                return err;
            }
        }

        files[newid - 1] = files[oldid - 1];
        return 0;
    }

    size_t posix_file_manager::seek(const fid id, const int off, const eka2l1::file_seek_mode whine, int &terrno) {
        if (id > files.size() || !id || !files[id - 1]) {
            terrno = EBADF;
            return 0;
        }

        const std::uint64_t result = files[id - 1]->handle->seek(off, whine);
        if (result == std::numeric_limits<std::uint64_t>::max()) {
            terrno = EINVAL;
            return 0;
        }

        terrno = 0;
        return static_cast<std::size_t>(result);
    }

    size_t posix_file_manager::tell(const fid id, int &terrno) {
        if (id > files.size() || !id || !files[id - 1]) {
            terrno = EBADF;
            return 0;
        }

        const std::uint64_t result = files[id - 1]->handle->tell();
        if (result == std::numeric_limits<std::uint64_t>::max()) {
            terrno = EIO;
            return 0;
        }

        terrno = 0;
        return static_cast<std::size_t>(result);
    }

    void posix_file_manager::stat(const fid id, posix_stat *filestat, int &terrno) {
        if (id > files.size() || !id || !files[id - 1]) {
            terrno = EBADF;
            return;
        }

        const std::shared_ptr<file> &file_handle = files[id - 1]->handle;
        const std::u16string full_path = file_handle->file_name();

        const std::optional<entry_info> info = io->get_entry_info(full_path);
        if (!info) {
            terrno = ENOENT;
            return;
        }

        *filestat = {};
        filestat->mode = POSIX_S_IFREG | 0777;
        filestat->link_count = 1;
        filestat->size = static_cast<std::int32_t>(std::min<std::size_t>(
            info->size, static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())));

        if (info->last_write >= common::ad_epoc_dist_microsecs) {
            const std::uint64_t unix_time = (info->last_write - common::ad_epoc_dist_microsecs) / common::microsecs_per_sec;
            filestat->modification_time = static_cast<std::int32_t>(std::min<std::uint64_t>(
                unix_time, static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())));
            filestat->access_time = filestat->modification_time;
            filestat->change_time = filestat->modification_time;
        }

        filestat->block_size = POSIX_BLOCK_SIZE;
        filestat->blocks = static_cast<std::int32_t>((info->size + POSIX_BLOCK_SIZE - 1) / POSIX_BLOCK_SIZE);
        terrno = 0;
    }

    size_t posix_file_manager::read(const fid id, const size_t len, char *buf, int &terrno) {
        if (id > files.size() || !id || !files[id - 1] || !files[id - 1]->readable) {
            terrno = EBADF;
            return 0;
        }

        const size_t res = files[id - 1]->handle->read_file(buf, 1, static_cast<std::uint32_t>(len));

        if (res == (size_t)-1) {
            terrno = EIO;
            return 0;
        }

        terrno = 0;
        return res;
    }

    size_t posix_file_manager::write(const fid id, const size_t len, char *buf, int &terrno) {
        if (id > files.size() || !id || !files[id - 1] || !files[id - 1]->writable) {
            terrno = EBADF;
            return 0;
        }

        const size_t res = files[id - 1]->handle->write_file(buf, 1, static_cast<std::uint32_t>(len));

        if (res == (size_t)-1) {
            terrno = EIO;
            return 0;
        }

        terrno = 0;
        return res;
    }

    void posix_server::chdir(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);

        const std::optional<std::u16string> current_dir = read_guest_path(own_process, params->cwptr[0]);
        if (!current_dir) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, EFAULT);
        }

        const std::u16string requested_dir = eka2l1::absolute_path(*current_dir, working_dir, true);
        io_system *io = ctx.sys->get_io_system();
        if (!io->exist(requested_dir)) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, ENOENT);
        }
        if (!io->is_directory(requested_dir)) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, ENOTDIR);
        }

        working_dir = requested_dir;

        params->ret = 0;

        POSIX_REQUEST_FINISH(ctx);
    }

    void posix_server::mkdir(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);

        const std::optional<std::u16string> current_dir = read_guest_path(own_process, params->cwptr[0]);
        if (!current_dir) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, EFAULT);
        }

        const std::u16string full_new_path = eka2l1::absolute_path(*current_dir, working_dir, true);

        io_system *io = ctx.sys->get_io_system();
        if (io->exist(full_new_path)) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, EEXIST);
        }

        const bool created = io->create_directories(full_new_path);
        params->ret = created ? 0 : -1;

        POSIX_REQUEST_FINISH_WITH_ERR(ctx, created ? 0 : EIO);
    }

    void posix_server::unlink(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);

        const std::optional<std::u16string> path = read_guest_path(own_process, params->cwptr[0]);
        if (!path) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, EFAULT);
        }

        const std::u16string full_path = eka2l1::absolute_path(*path, working_dir, true);
        io_system *io = ctx.sys->get_io_system();
        if (!io->exist(full_path)) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, ENOENT);
        }
        if (io->is_directory(full_path)) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, EISDIR);
        }

        const bool removed = io->delete_entry(full_path);
        params->ret = removed ? 0 : -1;
        POSIX_REQUEST_FINISH_WITH_ERR(ctx, removed ? 0 : EACCES);
    }

    void posix_server::open(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);

        std::u16string base_dir = working_dir;
        io_system *io = ctx.sys->get_io_system();

        // Get mode
        const int posix_open_mode = params->pint[0];
        if (posix_open_mode & POSIX_O_TMPFILE) {
            // Put the temporary file in tmp folder of the correspond private space of process in C drive
            base_dir = std::u16string(u"C:\\private\\")
                + common::utf8_to_ucs2(common::to_string(associated_process_uid, std::hex)) + u"\\tmp\\";
        }

        const std::optional<std::u16string> current_dir = read_guest_path(own_process, params->cwptr[0]);
        if (!current_dir) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, EFAULT);
        }

        const std::u16string path_u16 = eka2l1::absolute_path(*current_dir, base_dir, true);

        const std::int32_t access_mode = posix_open_mode & POSIX_O_ACCMODE;
        if ((access_mode != POSIX_O_RDONLY) && (access_mode != POSIX_O_WRONLY)
            && (access_mode != POSIX_O_RDWR)) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, EINVAL);
        }

        const bool temporary = (posix_open_mode & POSIX_O_TMPFILE) != 0;
        const bool create = temporary || ((posix_open_mode & POSIX_O_CREAT) != 0);
        const bool exists = io->exist(path_u16);

        if (exists && (posix_open_mode & POSIX_O_CREAT) && (posix_open_mode & POSIX_O_EXCL)) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, EEXIST);
        }

        if (!exists && !create) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, ENOENT);
        }

        if ((posix_open_mode & POSIX_O_TRUNC) && (access_mode == POSIX_O_RDONLY)) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, EINVAL);
        }

        if (temporary) {
            io->create_directories(base_dir);
        }

        // WRITE_MODE uses fopen's truncating/create form. Perform that step once,
        // then reopen read/write so dup() cannot accidentally truncate the file.
        if (!exists || temporary || (posix_open_mode & POSIX_O_TRUNC)) {
            symfile created = io->open_file(path_u16, WRITE_MODE | BIN_MODE);
            if (!created || !created->close()) {
                params->ret = -1;
                POSIX_REQUEST_FINISH_WITH_ERR(ctx, EIO);
            }
        }

        int emu_mode = BIN_MODE;
        if (access_mode == POSIX_O_RDONLY) {
            emu_mode |= READ_MODE;
        } else if (posix_open_mode & POSIX_O_APPEND) {
            emu_mode |= APPEND_MODE;
        } else {
            // The VFS has no write-only, non-truncating mode. rb+ supplies the
            // required write semantics without altering an existing file.
            emu_mode |= READ_MODE | WRITE_MODE;
        }

        // Open the associated file
        const bool readable = access_mode != POSIX_O_WRONLY;
        const bool writable = access_mode != POSIX_O_RDONLY;
        const fid file_id = file_manager.open(path_u16, emu_mode, readable, writable, *errnoptr);

        params->fid = file_id;
        params->ret = *errnoptr ? -1 : file_id;

        POSIX_REQUEST_FINISH(ctx);
    }

    void posix_server::close(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);

        file_manager.close(params->fid, *errnoptr);
        params->ret = *errnoptr ? -1 : 0;

        POSIX_REQUEST_FINISH(ctx);
    }

    void posix_server::lseek(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);

        eka2l1::file_seek_mode mode = eka2l1::file_seek_mode::beg;

        if (params->pint[1] == 1) {
            mode = eka2l1::file_seek_mode::crr;
        } else if (params->pint[1] == 2) {
            mode = eka2l1::file_seek_mode::end;
        } else if (params->pint[1] != 0) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, EINVAL);
        }

        params->ret = static_cast<std::int32_t>(file_manager.seek(params->fid, params->pint[0], mode, *errnoptr));

        if (*errnoptr) {
            params->ret = -1;
        }

        POSIX_REQUEST_FINISH(ctx);
    }

    void posix_server::fstat(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);

        if (!valid_guest_range(own_process, params->ptr[0].ptr_address(), sizeof(posix_stat))) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, EFAULT);
        }

        auto *file_stat = params->ptr[0].cast<posix_stat>().get(own_process);

        file_manager.stat(params->fid, file_stat, *errnoptr);
        params->ret = *errnoptr ? -1 : 0;

        POSIX_REQUEST_FINISH(ctx);
    }

    void posix_server::read(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);

        if (!valid_guest_range(own_process, params->ptr[0].ptr_address(), params->len[0])) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, EFAULT);
        }

        char *dest_ptr = params->ptr[0].get(own_process);
        params->ret = static_cast<std::int32_t>(file_manager.read(params->fid, params->len[0], dest_ptr, *errnoptr));

        if (*errnoptr) {
            params->ret = -1;
        }

        POSIX_REQUEST_FINISH(ctx);
    }

    void posix_server::write(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);

        if (!valid_guest_range(own_process, params->ptr[0].ptr_address(), params->len[0])) {
            params->ret = -1;
            POSIX_REQUEST_FINISH_WITH_ERR(ctx, EFAULT);
        }

        char *source_ptr = params->ptr[0].get(own_process);

        params->ret = static_cast<std::int32_t>(
            file_manager.write(params->fid, params->len[0], source_ptr, *errnoptr));

        if (*errnoptr) {
            params->ret = -1;
        }

        POSIX_REQUEST_FINISH(ctx);
    }

    void posix_server::dup(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);

        const auto new_fid = file_manager.duplicate(params->fid, *errnoptr);

        if (!new_fid) {
            params->ret = -1;
        } else {
            params->ret = *new_fid;
        }

        POSIX_REQUEST_FINISH(ctx);
    }

    void posix_server::dup2(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);

        *errnoptr = file_manager.duplicate_provide_fid(params->pint[0], params->fid);

        if (*errnoptr) {
            params->ret = -1;
        } else {
            params->ret = params->pint[0];
        }

        POSIX_REQUEST_FINISH(ctx);
    }

    posix_server::posix_server(eka2l1::system *sys, process_ptr &associated_process,
        const std::u16string &executable_path)
        : service::server(sys->get_kernel_system(), sys, nullptr, std::string("Posix-") + common::to_string(associated_process->unique_id()), true)
        , associated_process_uid(associated_process->get_uid())
        , file_manager(sys->get_io_system()) {
        update_executable_path(executable_path);

        REGISTER_IPC(posix_server, chdir, PMchdir, "Posix::Chdir");
        REGISTER_IPC(posix_server, mkdir, PMmkdir, "Posix::Mkdir");
        REGISTER_IPC(posix_server, unlink, PMunlink, "Posix::Unlink");
        REGISTER_IPC(posix_server, open, PMopen, "Posix::Open");
        REGISTER_IPC(posix_server, close, PMclose, "Posix::Close");
        REGISTER_IPC(posix_server, read, PMread, "Posix::Read");
        REGISTER_IPC(posix_server, write, PMwrite, "Posix::Write");
        REGISTER_IPC(posix_server, lseek, PMlseek, "Posix::LSeek");
        REGISTER_IPC(posix_server, fstat, PMfstat, "Posix::Fstat");
        REGISTER_IPC(posix_server, dup, PMdup, "Posix::Dup");
        REGISTER_IPC(posix_server, dup2, PMdup2, "Posix::Dup2");
    }

    void posix_server::update_executable_path(const std::u16string &executable_path) {
        working_dir = common::utf8_to_ucs2(eka2l1::root_name(
                          common::ucs2_to_utf8(executable_path), true))
            + u'\\';
    }
}
