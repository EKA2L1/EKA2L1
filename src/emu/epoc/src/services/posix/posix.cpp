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

#include <epoc/services/posix/op.h>
#include <epoc/services/posix/posix.h>

#include <common/cvt.h>
#include <common/e32inc.h>
#include <common/log.h>
#include <common/path.h>
#include <common/platform.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>

#include <epoc/utils/err.h>

/* These are in SDKS for ucrt on Windows SDK */
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#if EKA2L1_PLATFORM(POSIX)
#include <unistd.h>
#endif

#define POSIX_REQUEST_FINISH_WITH_ERR(ctx, err) \
    *errnoptr = err;                            \
    ctx.set_request_status(epoc::error_none);           \
    return

#define POSIX_REQUEST_FINISH(ctx)     \
    ctx.set_request_status(epoc::error_none); \
    return

#define POSIX_REQUEST_INIT(ctx)                                                                               \
    int *errnoptr = eka2l1::ptr<int>(ctx.msg->args.args[0]).get(ctx.sys->get_memory_system());                \
    *errnoptr = 0;                                                                                            \
    auto params = eka2l1::ptr<eka2l1::posix_params>(ctx.msg->args.args[1]).get(ctx.sys->get_memory_system()); \
    if (!params) {                                                                                            \
        POSIX_REQUEST_FINISH_WITH_ERR(ctx, EINVAL);                                                           \
    }

namespace eka2l1 {
    posix_file_manager::posix_file_manager(io_system *io)
        : io(io) {}

    fid posix_file_manager::get_lowest_useable_fid() {
        if (files.size() == MAX_FID) {
            return 0;
        }

        const auto &free_slot = std::find_if_not(files.begin(), files.end(),
            [](const symfile &f) -> bool { return f.get(); });

        if (free_slot == files.end()) {
            files.push_back(nullptr);
            return static_cast<fid>(files.size());
        }

        return static_cast<fid>(std::distance(files.begin(), free_slot)) + 1;
    }

    fid posix_file_manager::open(const std::u16string &path, const int mode, int &terrno) {
        const fid suit_fid = get_lowest_useable_fid();

        if (!suit_fid) {
            terrno = EMFILE;
            return 0;
        }

        files[suit_fid - 1] = io->open_file(path, mode);

        if (!files[suit_fid - 1]) {
            terrno = ENOENT;
            return 0;
        }

        terrno = 0;
        return suit_fid;
    }

    void posix_file_manager::close(const fid id, int &terrno) {
        if (id > files.size() || !id || !files[id - 1]) {
            terrno = EBADF;
            return;
        }

        const bool success = files[id - 1]->close();

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

        const fid suit = get_lowest_useable_fid();

        if (!suit) {
            terrno = EMFILE;
            return std::optional<fid>{};
        }

        // Open a new file descriptor with same attribute as last
        files[suit - 1] = io->open_file(files[id - 1]->file_name(), files[id - 1]->file_mode());
        return suit;
    }

    int posix_file_manager::duplicate_provide_fid(const fid newid, const fid oldid) {
        if (newid == oldid) {
            return 0;
        }

        if (oldid > files.size() || !oldid || !files[oldid - 1]) {
            return EBADF;
        }

        if (newid > files.size() || !newid) {
            return EBADF;
        }

        if (files[newid - 1]) {
            int err = 0;
            this->close(newid, err);

            if (err) {
                return err;
            }
        }

        files[newid - 1] = io->open_file(files[oldid - 1]->file_name(), files[oldid - 1]->file_mode());files[oldid - 1];
        return 0;
    }

    size_t posix_file_manager::seek(const fid id, const int off, const eka2l1::file_seek_mode whine, int &terrno) {
        if (id > files.size() || !id || !files[id - 1]) {
            terrno = EBADF;
            return 0;
        }

        return files[id - 1]->seek(off, whine);
    }

    size_t posix_file_manager::tell(const fid id, int &terrno) {
        if (id > files.size() || !id || !files[id - 1]) {
            terrno = EBADF;
            return 0;
        }

        return files[id - 1]->tell();
    }

    void posix_file_manager::stat(const fid id, struct stat *filestat, int &terrno) {
        if (id > files.size() || !id || !files[id - 1]) {
            terrno = EBADF;
            return;
        }

        symfile &f = files[id - 1];
        const std::u16string full_path = f->file_name();

        entry_info info = *io->get_entry_info(full_path);

        filestat->st_size = static_cast<off_t>(info.size);
        filestat->st_mode = static_cast<decltype(filestat->st_mode)>(0777);
        filestat->st_mtime = info.last_write / 1000000000;
    }

    size_t posix_file_manager::read(const fid id, const size_t len, char *buf, int &terrno) {
        if (id > files.size() || !id || !files[id - 1]) {
            terrno = EBADF;
            return 0;
        }

        size_t res = files[id - 1]->read_file(buf, 1, static_cast<std::uint32_t>(len));

        if (res == (size_t)-1) {
            terrno = EIO;
            return 0;
        }

        return res;
    }

    size_t posix_file_manager::write(const fid id, const size_t len, char *buf, int &terrno) {
        if (id > files.size() || !id || !files[id - 1]) {
            terrno = EBADF;
            return 0;
        }

        size_t res = files[id - 1]->write_file(buf, 1, static_cast<std::uint32_t>(len));

        if (res == (size_t)-1) {
            terrno = EIO;
            return 0;
        }

        return res;
    }

    void posix_server::chdir(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);

        std::u16string current_dir(reinterpret_cast<char16_t *>(
            params->cwptr[0].get(ctx.sys->get_memory_system())));

        working_dir = eka2l1::absolute_path(current_dir, working_dir, true);

        params->ret = 0;

        POSIX_REQUEST_FINISH(ctx);
    }

    void posix_server::mkdir(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);

        std::u16string current_dir(reinterpret_cast<char16_t *>(
            params->cwptr[0].get(ctx.sys->get_memory_system())));

        const std::u16string full_new_path = common::utf8_to_ucs2(eka2l1::absolute_path(
            common::ucs2_to_utf8(current_dir),
            common::ucs2_to_utf8(working_dir),
            true));

        bool res = ctx.sys->get_io_system()->create_directories(full_new_path);
        params->ret = res ? -1 : 0;

        POSIX_REQUEST_FINISH_WITH_ERR(ctx, res ? EIO : 0);
    }

    void posix_server::open(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);

        std::u16string base_dir = working_dir;
        io_system *io = ctx.sys->get_io_system();

        // Get mode
        const int posix_open_mode = params->pint[0];
        const int posix_open_perms = params->pint[1];

#if EKA2L1_PLATFORM(WIN32)
        if (posix_open_mode & O_TEMPORARY) {
#else
        if (posix_open_mode & O_TMPFILE) {
#endif
            // Put the temporary file in tmp folder of the correspond private space of process in C drive
            base_dir = std::u16string(u"C:\\private\\") + common::utf8_to_ucs2(common::to_string(associated_process->get_uid(), std::hex)) + u"\\tmp\\";
        }

        std::u16string current_dir(reinterpret_cast<char16_t *>(
            params->cwptr[0].get(ctx.sys->get_memory_system())));

        const std::u16string path_u16 = eka2l1::absolute_path(current_dir, base_dir, true);

        int write_flag_emu = APPEND_MODE;

        // If the file is temporary, override or create new one. Else
        // If the file exists and the flag is O_CREAT, also override existing file or create new one
        if ((!io->exist(path_u16) && (posix_open_mode & O_CREAT))
            || (posix_open_mode &
#if EKA2L1_PLATFORM(WIN32)
                O_TEMPORARY
#else
                O_TMPFILE
#endif
                )) {
            write_flag_emu = WRITE_MODE;
        }

        // Ignore the permissons right now, let's fetch the open mode
        int emu_mode = BIN_MODE;

        if (posix_open_mode & O_RDWR) {
            emu_mode |= (READ_MODE | write_flag_emu);
        } else if (posix_open_mode & O_WRONLY) {
            emu_mode |= write_flag_emu;
        } else {
            emu_mode |= READ_MODE;
        }

        // Open the associated file
        const fid file_id = file_manager.open(path_u16, emu_mode, *errnoptr);

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

        if (params->pint[1] & SEEK_CUR) {
            mode = eka2l1::file_seek_mode::crr;
        } else if (params->pint[1] & SEEK_END) {
            mode = eka2l1::file_seek_mode::end;
        }

        params->ret = static_cast<std::int32_t>(file_manager.seek(params->fid, params->pint[0], mode, *errnoptr));

        if (*errnoptr) {
            params->ret = -1;
        }

        POSIX_REQUEST_FINISH(ctx);
    }

    void posix_server::fstat(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);

        struct stat *file_stat = reinterpret_cast<struct stat *>(params->ptr[0].get(ctx.sys->get_memory_system()));

        file_manager.stat(params->fid, file_stat, *errnoptr);
        params->ret = *errnoptr ? -1 : 0;

        POSIX_REQUEST_FINISH(ctx);
    }

    void posix_server::read(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);
        params->ret = static_cast<std::int32_t>(file_manager.read(params->fid, params->len[0], params->ptr[0].get(ctx.sys->get_memory_system()), *errnoptr));

        if (*errnoptr) {
            params->ret = -1;
        }

        POSIX_REQUEST_FINISH(ctx);
    }

    void posix_server::write(service::ipc_context &ctx) {
        POSIX_REQUEST_INIT(ctx);
        params->ret = static_cast<std::int32_t>(
            file_manager.write(params->fid, params->len[0], params->ptr[0].get(ctx.sys->get_memory_system()), *errnoptr));

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

        if (errnoptr) {
            params->ret = -1;
        } else {
            params->ret = params->pint[0];
        }

        POSIX_REQUEST_FINISH(ctx);
    }

    posix_server::posix_server(eka2l1::system *sys, process_ptr &associated_process)
        : service::server(sys, std::string("Posix-") + common::to_string(associated_process->unique_id()), true)
        , associated_process(associated_process)
        , file_manager(sys->get_io_system()) {
        working_dir = common::utf8_to_ucs2(eka2l1::root_name(
                          common::ucs2_to_utf8(associated_process->get_exe_path()), true))
            + u'\\';

        REGISTER_IPC(posix_server, chdir, PMchdir, "Posix::Chdir");
        REGISTER_IPC(posix_server, mkdir, PMmkdir, "Posix::Mkdir");
        REGISTER_IPC(posix_server, open, PMopen, "Posix::Open");
        REGISTER_IPC(posix_server, close, PMclose, "Posix::Close");
        REGISTER_IPC(posix_server, read, PMread, "Posix::Read");
        REGISTER_IPC(posix_server, write, PMwrite, "Posix::Write");
        REGISTER_IPC(posix_server, lseek, PMlseek, "Posix::LSeek");
        REGISTER_IPC(posix_server, fstat, PMfstat, "Posix::Fstat");
        REGISTER_IPC(posix_server, dup, PMdup, "Posix::Dup");
        REGISTER_IPC(posix_server, dup2, PMdup2, "Posix::Dup2");
    }
}
