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

#include <epoc/epoc.h>
#include <epoc/kernel.h>
#include <epoc/vfs.h>

#include <epoc/utils/err.h>

namespace eka2l1 {
    size_t fs_path_case_insensitive_hasher::operator()(const utf16_str &key) const {
        utf16_str copy = common::lowercase_ucs2_string(key);
        return std::hash<utf16_str>()(copy);
    }

    bool fs_path_case_insensitive_comparer::operator()(const utf16_str &x, const utf16_str &y) const {
        return (common::compare_ignore_case(x, y) == -1);
    }

    fs_server_client::fs_server_client(service::typical_server *srv, kernel::uid suid, epoc::version client_version, service::ipc_context *ctx)
        : typical_session(srv, suid, client_version) {
        // Please don't remove the separator, absolute path needs this to determine root directory
        ss_path = eka2l1::root_name(ctx->msg->own_thr->owning_process()->get_exe_path(), true) + u'\\';
    }

    fs_server::fs_server(system *sys)
        : service::typical_server(sys, "!FileServer") {
        // Create property references to system drive
        // TODO (pent0): Not hardcode the drive. Maybe dangerous, who knows.
        system_drive_prop = &(*sys->get_kernel_system()->create<service::property>());
        system_drive_prop->define(service::property_type::int_data, 0);
        system_drive_prop->set(drive_c);

        system_drive_prop->first = static_cast<int>(FS_UID);
        system_drive_prop->second = static_cast<int>(SYSTEM_DRIVE_KEY);
    }

    void fs_server_client::fetch(service::ipc_context *ctx) {
        const epocver version = server<fs_server>()->sys->get_symbian_version_use();

        if (version <= epocver::epoc93) {
            // FileWriteDirty does not exist
            if (ctx->msg->function >= epoc::fs_msg_file_write_dirty)
                ctx->msg->function++;
        }

        switch (ctx->msg->function & 0xFF) {
// For debug purpose, uncomment the log
#define HANDLE_CLIENT_IPC(name, op, debug_func_str)     \
    case (op): {                                        \
        name(ctx); /*LOG_TRACE("{}", debug_func_str);*/ \
        break;                                          \
    }

            HANDLE_CLIENT_IPC(entry, epoc::fs_msg_entry, "Fs::Entry");
            HANDLE_CLIENT_IPC(file_open, epoc::fs_msg_file_open, "Fs::FileOpen");
            HANDLE_CLIENT_IPC(file_size, epoc::fs_msg_file_size, "Fs::FileSize");
            HANDLE_CLIENT_IPC(file_set_size, epoc::fs_msg_file_set_size, "Fs::FileSetSize");
            HANDLE_CLIENT_IPC(file_seek, epoc::fs_msg_file_seek, "Fs::FileSeek");
            HANDLE_CLIENT_IPC(file_read, epoc::fs_msg_file_read, "Fs::FileRead");
            HANDLE_CLIENT_IPC(file_write, epoc::fs_msg_file_write, "Fs::FileWrite");
            HANDLE_CLIENT_IPC(file_flush, epoc::fs_msg_file_flush, "Fs::FileFlush");
            HANDLE_CLIENT_IPC(file_temp, epoc::fs_msg_file_temp, "Fs::FileTemp");
            HANDLE_CLIENT_IPC(file_duplicate, epoc::fs_msg_file_duplicate, "Fs::FileDuplicate");
            HANDLE_CLIENT_IPC(file_adopt, epoc::fs_msg_file_adopt, "Fs::FileAdopt");
            HANDLE_CLIENT_IPC(file_rename, epoc::fs_msg_file_rename, "Fs::FileRename(Move)");
            HANDLE_CLIENT_IPC(file_replace, epoc::fs_msg_file_replace, "Fs::FileReplace");
            HANDLE_CLIENT_IPC(file_create, epoc::fs_msg_file_create, "Fs::FileCreate");
            HANDLE_CLIENT_IPC(file_close, epoc::fs_msg_file_subclose, "Fs::FileSubClose");
            HANDLE_CLIENT_IPC(file_drive, epoc::fs_msg_file_drive, "Fs::FileDrive");
            HANDLE_CLIENT_IPC(file_name, epoc::fs_msg_filename, "Fs::FileName");
            HANDLE_CLIENT_IPC(file_full_name, epoc::fs_msg_file_fullname, "Fs::FileFullName");
            HANDLE_CLIENT_IPC(file_att, epoc::fs_msg_file_att, "Fs::FileAtt");
            HANDLE_CLIENT_IPC(file_modified, epoc::fs_msg_file_modified, "Fs::FileModified");
            HANDLE_CLIENT_IPC(is_file_in_rom, epoc::fs_msg_is_file_in_rom, "Fs::IsFileInRom");
            HANDLE_CLIENT_IPC(open_dir, epoc::fs_msg_dir_open, "Fs::OpenDir");
            HANDLE_CLIENT_IPC(close_dir, epoc::fs_msg_dir_subclose, "Fs::CloseDir");
            HANDLE_CLIENT_IPC(read_dir, epoc::fs_msg_dir_read_one, "Fs::ReadDir");
            HANDLE_CLIENT_IPC(read_dir_packed, epoc::fs_msg_dir_read_packed, "Fs::ReadDirPacked");
            HANDLE_CLIENT_IPC(session_path, epoc::fs_msg_session_path, "Fs::SessionPath");
            HANDLE_CLIENT_IPC(set_session_path, epoc::fs_msg_set_session_path, "Fs::SetSessionPath");
            HANDLE_CLIENT_IPC(set_session_to_private, epoc::fs_msg_session_to_private, "Fs::SetSessionToPrivate");
            HANDLE_CLIENT_IPC(create_private_path, epoc::fs_msg_create_private_path, "Fs::CreatePrivatePath");
            HANDLE_CLIENT_IPC(notify_change_ex, epoc::fs_msg_notify_change_ex, "Fs::NotifyChangeEx");
            HANDLE_CLIENT_IPC(notify_change, epoc::fs_msg_notify_change, "Fs::NotifyChange");
            HANDLE_CLIENT_IPC(mkdir, epoc::fs_msg_mkdir, "Fs::MkDir");
            HANDLE_CLIENT_IPC(rmdir, epoc::fs_msg_rmdir, "Fs::RmDir");
            HANDLE_CLIENT_IPC(delete_entry, epoc::fs_msg_delete, "Fs::Delete");
            HANDLE_CLIENT_IPC(set_entry, epoc::fs_msg_set_entry, "Fs::SetEntry");
            HANDLE_CLIENT_IPC(rename, epoc::fs_msg_rename, "Fs::Rename(Move)");
            HANDLE_CLIENT_IPC(replace, epoc::fs_msg_replace, "Fs::Replace");
            HANDLE_CLIENT_IPC(read_file_section, epoc::fs_msg_read_file_section, "Fs::ReadFileSection");
            HANDLE_CLIENT_IPC(set_should_notify_failure, epoc::fs_msg_set_notify_user, "Fs::SetShouldNotifyFailure");
            HANDLE_CLIENT_IPC(server<fs_server>()->drive_list, epoc::fs_msg_drive_list, "Fs::DriveList");
            HANDLE_CLIENT_IPC(server<fs_server>()->drive, epoc::fs_msg_drive, "Fs::Drive");
            HANDLE_CLIENT_IPC(server<fs_server>()->synchronize_driver, epoc::fs_msg_sync_drive_thread, "Fs::SyncDriveThread");
            HANDLE_CLIENT_IPC(server<fs_server>()->private_path, epoc::fs_msg_private_path, "Fs::PrivatePath");
            HANDLE_CLIENT_IPC(server<fs_server>()->volume, epoc::fs_msg_volume, "Fs::Volume");
            HANDLE_CLIENT_IPC(server<fs_server>()->query_drive_info_ext, epoc::fs_msg_query_volume_info_ext, "Fs::QueryVolumeInfoExt");

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

    static std::u16string get_private_path(kernel::process *pr, const drive_number drive) {
        const char16_t drive_dos_char = char16_t(0x41 + static_cast<int>(drive));
        const std::u16string drive_u16 = std::u16string(&drive_dos_char, 1) + u":";

        // Try to get the app uid
        uint32_t uid = std::get<2>(pr->get_uid_type());
        std::string hex_id = common::to_string(uid, std::hex);

        return drive_u16 + u"\\Private\\" + common::utf8_to_ucs2(hex_id) + u"\\";
    }

    void fs_server_client::set_session_to_private(service::ipc_context *ctx) {
        auto drive_ordinal = ctx->get_arg<std::int32_t>(0);

        if (!drive_ordinal) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        ss_path = get_private_path(ctx->msg->own_thr->owning_process(), static_cast<drive_number>(drive_ordinal.value()));
        ctx->set_request_status(epoc::error_none);
    }

    void fs_server_client::create_private_path(service::ipc_context *ctx) {
        const std::u16string private_path = get_private_path(ctx->msg->own_thr->owning_process(),
            static_cast<drive_number>(ctx->get_arg<std::int32_t>(0).value()));

        eka2l1::io_system *io = ctx->sys->get_io_system();

        if (io->exist(private_path)) {
            ctx->set_request_status(epoc::error_already_exists);
            return;
        }

        if (!io->create_directory(private_path)) {
            ctx->set_request_status(epoc::error_permission_denied);
            return;
        }

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

        if (!f) {
            ctx->set_request_status(0);
            return;
        }

        address addr = f->rom_address();

        f->close();

        ctx->write_arg_pkg<address>(1, addr);
        ctx->set_request_status(epoc::error_none);
    }

    void fs_server_client::notify_change(service::ipc_context *ctx) {
        notify_entry entry;

        entry.match_pattern = ".*";
        entry.type = static_cast<notify_type>(*ctx->get_arg<std::int32_t>(0));
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
        entry.type = static_cast<notify_type>(*ctx->get_arg<std::int32_t>(0));
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

        if (*ctx->get_arg<std::int32_t>(1)) {
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

    void fs_server_client::rmdir(service::ipc_context *ctx) {
        std::optional<std::u16string> dir = ctx->get_arg<std::u16string>(0);

        if (!dir) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        io_system *io = ctx->sys->get_io_system();
        io->delete_entry(dir.value());

        ctx->set_request_status(epoc::error_none);
    }

    std::uint32_t build_attribute_from_entry_info(entry_info &info) {
        std::uint32_t attrib = epoc::fs::entry_att_normal;

        if (info.has_raw_attribute) {
            attrib = info.raw_attribute;
        } else {
            bool dir = (info.type == io_component_type::dir);

            if (static_cast<int>(info.attribute) & static_cast<int>(io_attrib::internal)) {
                attrib |= epoc::fs::entry_att_read_only | epoc::fs::entry_att_system;
            }

            // TODO (pent0): Mark the file as XIP if is ROM image (probably ROM already did it, but just be cautious).

            if (dir) {
                attrib |= epoc::fs::entry_att_dir;
            } else {
                attrib |= epoc::fs::entry_att_archive;
            }
        }

        return attrib;
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
        entry.size_high = static_cast<std::uint32_t>(entry_hle->size >> 32);

        entry.attrib = build_attribute_from_entry_info(entry_hle.value());
        entry.name = fname;
        entry.modified = epoc::time{ entry_hle->last_write };

        ctx->write_arg_pkg<epoc::fs::entry>(1, entry);
        ctx->set_request_status(epoc::error_none);
    }

    void fs_server_client::set_entry(service::ipc_context *ctx) {
        std::optional<std::u16string> fname_op = ctx->get_arg<std::u16string>(0);
        std::optional<epoc::time> time = ctx->get_arg_packed<epoc::time>(1);
        std::uint32_t set_att_mask = *ctx->get_arg<std::uint32_t>(2);
        std::uint32_t clear_att_mask = *ctx->get_arg<std::uint32_t>(3);

        if (!fname_op) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        std::u16string fname = std::move(*fname_op);
        fname = eka2l1::absolute_path(fname, ss_path, true);

        LOG_INFO("Set entry of: {}", common::ucs2_to_utf8(fname));

        io_system *io = ctx->sys->get_io_system();

        std::optional<entry_info> entry_hle = io->get_entry_info(fname);

        if (!entry_hle) {
            ctx->set_request_status(epoc::error_not_found);
            return;
        }

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
        should_notify_failures = static_cast<bool>(ctx->get_arg<std::int32_t>(0));
        ctx->set_request_status(epoc::error_none);
    }
}
