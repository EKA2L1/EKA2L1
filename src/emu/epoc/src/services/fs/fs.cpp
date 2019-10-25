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

#include <epoc/services/fs/fs.h>
#include <epoc/services/fs/op.h>
#include <epoc/services/fs/std.h>

#include <epoc/utils/des.h>

#include <clocale>
#include <cwctype>
#include <memory>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/wildcard.h>

#include <common/e32inc.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>
#include <epoc/vfs.h>

#include <epoc/utils/err.h>

namespace eka2l1 {
    size_t fs_path_case_insensitive_hasher::operator()(const utf16_str &key) const {
        utf16_str copy(key);

        std::locale loc("");
        for (auto &wc : copy) {
            wc = std::tolower(wc, loc);
        }

        return std::hash<utf16_str>()(copy);
    }

    bool fs_path_case_insensitive_comparer::operator()(const utf16_str &x, const utf16_str &y) const {
        return (common::compare_ignore_case(x, y) == -1);
    }

    fs_server_client::fs_server_client(service::typical_server *srv, kernel::uid suid, service::ipc_context *ctx)
        : typical_session(srv, suid) {     
        // Please don't remove the seperator, absolute path needs this to determine root directory
        ss_path = eka2l1::root_name(ctx->msg->own_thr->owning_process()->get_exe_path(), true) + u'\\';
    }

    fs_server::fs_server(system *sys) : service::typical_server(sys, "!FileServer") {
        // Create property references to system drive
        // TODO (pent0): Not hardcode the drive. Maybe dangerous, who knows.
        system_drive_prop = &(*sys->get_kernel_system()->create<service::property>());
        system_drive_prop->define(service::property_type::int_data, 0);
        system_drive_prop->set(drive_c);

        system_drive_prop->first = static_cast<int>(FS_UID);
        system_drive_prop->second = static_cast<int>(SYSTEM_DRIVE_KEY);
    }

    void fs_server_client::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        // For debug purpose, uncomment the log
        #define HANDLE_CLIENT_IPC(name, op, debug_func_str)                                         \
            case (op): { name(ctx); /*LOG_TRACE("{}", debug_func_str);*/ break; }

        HANDLE_CLIENT_IPC(entry, EFsEntry, "Fs::Entry");
        HANDLE_CLIENT_IPC(file_open, EFsFileOpen, "Fs::FileOpen");
        HANDLE_CLIENT_IPC(file_size, EFsFileSize, "Fs::FileSize");
        HANDLE_CLIENT_IPC(file_set_size, EFsFileSetSize, "Fs::FileSetSize");
        HANDLE_CLIENT_IPC(file_seek, EFsFileSeek, "Fs::FileSeek");
        HANDLE_CLIENT_IPC(file_read, EFsFileRead, "Fs::FileRead");
        HANDLE_CLIENT_IPC(file_write, EFsFileWrite, "Fs::FileWrite");
        HANDLE_CLIENT_IPC(file_flush, EFsFileFlush, "Fs::FileFlush");
        HANDLE_CLIENT_IPC(file_temp, EFsFileTemp, "Fs::FileTemp");
        HANDLE_CLIENT_IPC(file_duplicate, EFsFileDuplicate, "Fs::FileDuplicate");
        HANDLE_CLIENT_IPC(file_adopt, EFsFileAdopt, "Fs::FileAdopt");
        HANDLE_CLIENT_IPC(file_rename, EFsFileRename, "Fs::FileRename(Move)");
        HANDLE_CLIENT_IPC(file_replace, EFsFileReplace, "Fs::FileReplace");
        HANDLE_CLIENT_IPC(file_create, EFsFileCreate, "Fs::FileCreate");
        HANDLE_CLIENT_IPC(file_close, EFsFileSubClose, "Fs::FileSubClose");
        HANDLE_CLIENT_IPC(file_drive, EFsFileDrive, "Fs::FileDrive");
        HANDLE_CLIENT_IPC(file_name, EFsFileName, "Fs::FileName");
        HANDLE_CLIENT_IPC(file_full_name, EFsFileFullName, "Fs::FileFullName");
        HANDLE_CLIENT_IPC(is_file_in_rom, EFsIsFileInRom, "Fs::IsFileInRom");
        HANDLE_CLIENT_IPC(open_dir, EFsDirOpen, "Fs::OpenDir");
        HANDLE_CLIENT_IPC(close_dir, EFsDirSubClose, "Fs::CloseDir");
        HANDLE_CLIENT_IPC(read_dir, EFsDirReadOne, "Fs::ReadDir");
        HANDLE_CLIENT_IPC(read_dir_packed, EFsDirReadPacked, "Fs::ReadDirPacked");
        HANDLE_CLIENT_IPC(session_path, EFsSessionPath, "Fs::SessionPath");
        HANDLE_CLIENT_IPC(set_session_path, EFsSetSessionPath, "Fs::SetSessionPath");
        HANDLE_CLIENT_IPC(set_session_to_private, EFsSessionToPrivate, "Fs::SetSessionToPrivate");
        HANDLE_CLIENT_IPC(notify_change_ex, EFsNotifyChangeEx, "Fs::NotifyChangeEx");
        HANDLE_CLIENT_IPC(notify_change, EFsNotifyChange, "Fs::NotifyChange");
        HANDLE_CLIENT_IPC(mkdir, EFsMkDir, "Fs::MkDir");
        HANDLE_CLIENT_IPC(delete_entry, EFsDelete, "Fs::Delete");
        HANDLE_CLIENT_IPC(rename, EFsRename, "Fs::Rename(Move)");
        HANDLE_CLIENT_IPC(replace, EFsReplace, "Fs::Replace");
        HANDLE_CLIENT_IPC(set_should_notify_failure, EFsSetNotifyUser, "Fs::SetShouldNotifyFailure");
        HANDLE_CLIENT_IPC(server<fs_server>()->drive_list, EFsDriveList, "Fs::DriveList");
        HANDLE_CLIENT_IPC(server<fs_server>()->drive, EFsDrive, "Fs::Drive");
        HANDLE_CLIENT_IPC(server<fs_server>()->synchronize_driver, EFsSynchroniseDriveThread, "Fs::SyncDriveThread");
        HANDLE_CLIENT_IPC(server<fs_server>()->private_path, EFsPrivatePath, "Fs::PrivatePath");
        HANDLE_CLIENT_IPC(server<fs_server>()->volume, EFsVolume, "Fs::Volume");
        HANDLE_CLIENT_IPC(server<fs_server>()->query_drive_info_ext, EFsQueryVolumeInfoExt, "Fs::QueryVolumeInfoExt");

        default: {
            LOG_ERROR("Unknown FSServer client opcode {}!", ctx->msg->function);
            break;
        }

        #undef HANDLE_CLIENT_IPC           
        }
    }

    void fs_server_client::replace(service::ipc_context *ctx) {
        auto given_path_target = ctx->get_arg<std::u16string>(0);
        auto given_path_dest = ctx->get_arg<std::u16string>(1);

        if (!given_path_target || !given_path_dest) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        auto target = eka2l1::absolute_path(*given_path_target, ss_path, true);
        auto dest = eka2l1::absolute_path(*given_path_dest, ss_path, true);

        io_system *io = ctx->sys->get_io_system();

        // If exists, delete it so the new file can be replaced
        if (io->exist(dest)) {
            io->delete_entry(dest);
        }

        bool res = io->rename(target, dest);

        if (!res) {
            ctx->set_request_status(epoc::error_general);
            return;
        }

        // A new app list may be created
        ctx->set_request_status(epoc::error_none);
    }

    void fs_server_client::rename(service::ipc_context *ctx) {
        auto given_path_target = ctx->get_arg<std::u16string>(0);
        auto given_path_dest = ctx->get_arg<std::u16string>(1);

        if (!given_path_target || !given_path_dest) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        std::u16string target = eka2l1::absolute_path(*given_path_target, ss_path, true);
        std::u16string dest = eka2l1::absolute_path(*given_path_dest, ss_path, true);

        io_system *io = ctx->sys->get_io_system();

        if (io->exist(dest)) {
            ctx->set_request_status(epoc::error_already_exists);
            return;
        }

        bool res = io->rename(target, dest);

        if (!res) {
            ctx->set_request_status(epoc::error_general);
            return;
        }

        // A new app list may be created
        ctx->set_request_status(epoc::error_none);
    }

    void fs_server_client::delete_entry(service::ipc_context *ctx) {
        auto given_path = ctx->get_arg<std::u16string>(0);

        if (!given_path) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        auto path = eka2l1::absolute_path(*given_path, ss_path, true);
        io_system *io = ctx->sys->get_io_system();

        bool success = io->delete_entry(path);

        if (!success) {
            ctx->set_request_status(epoc::error_not_found);
            return;
        }

        ctx->set_request_status(epoc::error_none);
    }

    void fs_server::synchronize_driver(service::ipc_context *ctx) {
        ctx->set_request_status(epoc::error_none);
    }

    void fs_server::connect(service::ipc_context &ctx) {
        create_session<fs_server_client>(&ctx, &ctx);
        typical_server::connect(ctx);
    }

    void fs_server::disconnect(service::ipc_context &ctx) {
        typical_server::disconnect(ctx);
    }

    void fs_server_client::session_path(service::ipc_context *ctx) {
        ctx->write_arg(0, ss_path);
        ctx->set_request_status(epoc::error_none);
    }

    void fs_server_client::set_session_path(service::ipc_context *ctx) {
        auto new_path = ctx->get_arg<std::u16string>(0);

        if (!new_path) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        ss_path = std::move(new_path.value());
        ctx->set_request_status(epoc::error_none);
    }

    void fs_server_client::set_session_to_private(service::ipc_context *ctx) {
        auto drive_ordinal = ctx->get_arg<int>(0);

        if (!drive_ordinal) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        char16_t drive_dos_char = char16_t(0x41 + *drive_ordinal);
        std::u16string drive_u16 = std::u16string(&drive_dos_char, 1) + u":";

        // Try to get the app uid
        uint32_t uid = std::get<2>(ctx->msg->own_thr->owning_process()->get_uid_type());
        std::string hex_id = common::to_string(uid, std::hex);

        ss_path = drive_u16 + u"\\Private\\" + common::utf8_to_ucs2(hex_id) + u"\\";
        ctx->set_request_status(epoc::error_none);
    }

    void fs_server_client::is_file_in_rom(service::ipc_context *ctx) {
        std::optional<utf16_str> path = ctx->get_arg<utf16_str>(0);

        if (!path) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        auto final_path = std::move(*path);

        if (!eka2l1::is_absolute(final_path, ss_path, true)) {
            final_path = eka2l1::absolute_path(final_path, ss_path, true);
        }

        symfile f = ctx->sys->get_io_system()->open_file(final_path, READ_MODE);
        address addr = f->rom_address();

        f->close();

        ctx->write_arg_pkg<address>(1, addr);
        ctx->set_request_status(epoc::error_none);
    }

    void fs_server_client::notify_change(service::ipc_context *ctx) {
        notify_entry entry;

        entry.match_pattern = ".*";
        entry.type = static_cast<notify_type>(*ctx->get_arg<int>(0));
        entry.request_status = ctx->msg->request_sts;
        entry.request_thread = ctx->msg->own_thr;

        notify_entries.push_back(entry);
    }

    void fs_server_client::notify_change_ex(service::ipc_context *ctx) {
        std::optional<utf16_str> wildcard_match = ctx->get_arg<utf16_str>(1);

        if (!wildcard_match) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        notify_entry entry;
        entry.match_pattern = common::wildcard_to_regex_string(common::ucs2_to_utf8(*wildcard_match));
        entry.type = static_cast<notify_type>(*ctx->get_arg<int>(0));
        entry.request_status = ctx->msg->request_sts;
        entry.request_thread = ctx->msg->own_thr;

        notify_entries.push_back(entry);

        LOG_TRACE("Notify requested with wildcard: {}", common::ucs2_to_utf8(*wildcard_match));
    }

    bool is_e32img(symfile f) {
        int sig;

        f->seek(12, file_seek_mode::beg);
        f->read_file(&sig, 1, 4);

        if (sig != 0x434F5045) {
            return false;
        }

        return true;
    }

    void fs_server_client::mkdir(service::ipc_context *ctx) {
        std::optional<std::u16string> dir = ctx->get_arg<std::u16string>(0);

        if (!dir) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        bool res = false;

        if (*ctx->get_arg<int>(1)) {
            res = ctx->sys->get_io_system()->create_directories(eka2l1::file_directory(*dir));
        } else {
            res = ctx->sys->get_io_system()->create_directory(eka2l1::file_directory(*dir));
        }

        if (!res) {
            // The guide specified: if it's parent does not exist or the sub-directory
            // already created, this should returns
            ctx->set_request_status(epoc::error_already_exists);
            return;
        }

        ctx->set_request_status(epoc::error_none);
    }

    void fs_server_client::entry(service::ipc_context *ctx) {
        std::optional<std::u16string> fname_op = ctx->get_arg<std::u16string>(0);

        if (!fname_op) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        std::u16string fname = std::move(*fname_op);
        fname = eka2l1::absolute_path(fname, ss_path, true);

        LOG_INFO("Get entry of: {}", common::ucs2_to_utf8(fname));

        bool dir = false;

        io_system *io = ctx->sys->get_io_system();

        std::optional<entry_info> entry_hle = io->get_entry_info(fname);

        if (!entry_hle) {
            ctx->set_request_status(epoc::error_not_found);
            return;
        }

        epoc::fs::entry entry;
        entry.size = static_cast<std::uint32_t>(entry_hle->size);

        if (entry_hle->has_raw_attribute) {
            entry.attrib = entry_hle->raw_attribute;
        } else {
            bool dir = (entry_hle->type == io_component_type::dir);

            if (static_cast<int>(entry_hle->attribute) & static_cast<int>(io_attrib::internal)) {
                entry.attrib = epoc::fs::entry_att_read_only | epoc::fs::entry_att_system;
            }

            // TODO (pent0): Mark the file as XIP if is ROM image (probably ROM already did it, but just be cautious).

            if (dir) {
                entry.attrib |= epoc::fs::entry_att_dir;
            } else {
                entry.attrib |= epoc::fs::entry_att_archive;
            }
        }

        entry.size_high = 0;
        entry.name = fname;
        entry.modified = epoc::time { entry_hle->last_write };

        ctx->write_arg_pkg<epoc::fs::entry>(1, entry);
        ctx->set_request_status(epoc::error_none);
    }

    void fs_server::private_path(service::ipc_context *ctx) {
        std::u16string path = u"\\private\\"
            + common::utf8_to_ucs2(common::to_string(std::get<2>(ctx->msg->own_thr->owning_process()->get_uid_type()), std::hex))
            + u"\\";

        ctx->write_arg(0, path);
        ctx->set_request_status(epoc::error_none);
    }

    void fs_server_client::set_should_notify_failure(service::ipc_context *ctx) {
        should_notify_failures = static_cast<bool>(ctx->get_arg<int>(0));
        ctx->set_request_status(epoc::error_none);
    }
}
