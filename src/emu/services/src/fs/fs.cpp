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

#include <services/fs/fs.h>
#include <services/fs/op.h>
#include <services/fs/parser.h>
#include <services/fs/std.h>

#include <utils/des.h>

#include <clocale>
#include <cwctype>
#include <memory>
#include <regex>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/wildcard.h>

#include <kernel/kernel.h>
#include <system/epoc.h>
#include <vfs/vfs.h>

#include <common/path.h>
#include <common/pystr.h>
#include <utils/err.h>

#include <services/fs/sec.h>

namespace eka2l1 {
    bool check_path_capabilities_pass(const std::u16string &path, kernel::process *pr, epoc::security_policy &private_policy, epoc::security_policy &sys_policy, epoc::security_policy &resource_policy) {
        if (!pr->get_kernel_object_owner()->support_capabilities()) {
            return true;
        }

        const std::u16string path_lowered = common::lowercase_ucs2_string(path);
        eka2l1::path_iterator_16 iterator(path_lowered);

        std::int8_t policy_checK_type = -1;

        // Skip the drive part...
        iterator++;

        if (iterator) {
            if (*iterator == u"private") {
                policy_checK_type = 0;
            } else if (*iterator == u"sys") {
                policy_checK_type = 1;
            } else if (*iterator == u"resource") {
                policy_checK_type = 2;
            }

            if (policy_checK_type != -1) {
                if (iterator)
                    iterator++;
            }
        }

        if ((!iterator) || (policy_checK_type == -1)) {
            // No more components up ahead, it's a nice pass
            return true;
        }

        switch (policy_checK_type) {
        case 0: {
            common::pystr16 hexstr(*iterator);
            const std::uint32_t sid_in_path = hexstr.as_int<std::uint32_t>(0, 16);

            if (sid_in_path != pr->get_sec_info().secure_id) {
                return pr->satisfy(private_policy);
            }

            return true;
        }

        case 1:
            return pr->satisfy(sys_policy);

        case 2:
            return pr->satisfy(resource_policy);

        default:
            break;
        }

        return true;
    }

    static std::u16string get_private_path_trim_uid(kernel::process *pr) {
        // Try to get the app uid
        uint32_t uid = std::get<2>(pr->get_uid_type());
        std::string hex_id = common::uppercase_string(common::to_string(uid, std::hex));

        return u"\\Private\\" + common::utf8_to_ucs2(hex_id) + u"\\";
    }

    static std::u16string get_private_path(kernel::process *pr, const drive_number drive) {
        const char16_t drive_dos_char = drive_to_char16(drive);
        const std::u16string drive_u16 = std::u16string(&drive_dos_char, 1) + u":";

        return drive_u16 + get_private_path_trim_uid(pr);
    }

    std::u16string get_full_symbian_path(const std::u16string &session_path, const std::u16string &target_path) {
        if (target_path.empty()) {
            return session_path;
        }

        if (is_separator(target_path[0])) {
            // Only append the root directory to the beginning
            return eka2l1::add_path(eka2l1::root_name(session_path, true), target_path, true);
        }

        // Check if the path has a root directory
        if (!eka2l1::has_root_dir(target_path)) {
            return eka2l1::add_path(session_path, target_path, true);
        }

        return target_path;
    }

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
        kernel::process *pr = ctx->msg->own_thr->owning_process();
        const std::u16string root_name = eka2l1::root_name(pr->get_exe_path());

        if (server<fs_server>()->kern->is_eka1()) {
            ss_path = server<fs_server>()->default_sys_path;
        } else {
            // The default session path is private path with system drive, see sf_main.cpp in sfile module, line 122
            ss_path = get_private_path(pr, static_cast<drive_number>(server<fs_server>()->system_drive_prop->get_int()));
        }
    }

    fs_server::fs_server(system *sys)
        : service::typical_server(sys, epoc::fs::get_server_name_through_epocver(sys->get_symbian_version_use()))
        , flags(0) {
        // Create property references to system drive
        // TODO (pent0): Not hardcode the drive. Maybe dangerous, who knows.
        default_sys_path = u"C:\\";
        system_drive_prop = &(*sys->get_kernel_system()->create<service::property>());
        system_drive_prop->define(service::property_type::int_data, 0);
        system_drive_prop->set_int(drive_c);

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

        if (version < epocver::eka2) {
            if ((ctx->msg->function >= epoc::fs_msg_transition_begin) && (version >= epocver::epoc81a)) {
                const epoc::fs_message quick_lookup[] = {
                    epoc::fs_msg_swap_filesystem,
                    epoc::fs_msg_add_ext,
                    epoc::fs_msg_mount_ext,
                    epoc::fs_msg_dismount_ext,
                    epoc::fs_msg_remove_ext,
                    epoc::fs_msg_ext_name,
                    epoc::fs_msg_startup_init_complete,
                    epoc::fs_msg_raw_disk_write,
                    epoc::fs_msg_max_client_operations, // virus scanner opcode
                    epoc::fs_msg_session_to_private,
                    epoc::fs_msg_private_path,
                    epoc::fs_msg_create_private_path,
                    epoc::fs_msg_reserve_drive_space,
                    epoc::fs_msg_get_reserve_access,
                    epoc::fs_msg_release_reserve_access,
                    epoc::fs_msg_erase_password,

                    // Add some more for reservations of OOB
                    epoc::fs_msg_max_client_operations,
                    epoc::fs_msg_max_client_operations,
                    epoc::fs_msg_max_client_operations,
                    epoc::fs_msg_max_client_operations,
                    epoc::fs_msg_max_client_operations,
                    epoc::fs_msg_max_client_operations
                };

                ctx->msg->function = quick_lookup[ctx->msg->function - epoc::fs_msg_transition_begin];
            } else {
                // Base subclose, see RFsBase from devpedia
                if (ctx->msg->function > epoc::fs_msg_base_close) {
                    // Skip to open. EKA2 separate RFsBase into separate categories
                    ctx->msg->function += epoc::fs_msg_raw_subclose - epoc::fs_msg_base_close;
                }

                if (ctx->msg->function > epoc::fs_msg_debug_function) {
                    // No debug function opcode
                    ctx->msg->function -= 1;
                }
            }
        }

        switch (ctx->msg->function & 0xFF) {
// For debug purpose, uncomment the log
#define HANDLE_CLIENT_IPC(name, op, debug_func_str)                    \
    case (op): {                                                       \
        name(ctx); /*LOG_TRACE(SERVICE_EFSRV, "{}", debug_func_str);*/ \
        break;                                                         \
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
            HANDLE_CLIENT_IPC(file_set_att, epoc::fs_msg_file_set_att, "Fs::FileSetAtt");
            HANDLE_CLIENT_IPC(file_modified, epoc::fs_msg_file_modified, "Fs::FileModified");
            HANDLE_CLIENT_IPC(file_set_modified, epoc::fs_msg_file_set_modified, "Fs::FileSetModified");
            HANDLE_CLIENT_IPC(is_file_in_rom, epoc::fs_msg_is_file_in_rom, "Fs::IsFileInRom");
            HANDLE_CLIENT_IPC(is_valid_name, epoc::fs_msg_is_valid_name, "Fs::IsValidName");
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
            HANDLE_CLIENT_IPC(notify_change_cancel, epoc::fs_msg_notify_change_cancel, "Fs::NotifyChangeCancel"); // Same implementation on emulator.
            HANDLE_CLIENT_IPC(notify_change_cancel_ex, epoc::fs_msg_notify_change_cancel_ex, "Fs::NotifyChangeCancelEx");
            HANDLE_CLIENT_IPC(mkdir, epoc::fs_msg_mkdir, "Fs::MkDir");
            HANDLE_CLIENT_IPC(rmdir, epoc::fs_msg_rmdir, "Fs::RmDir");
            HANDLE_CLIENT_IPC(parse, epoc::fs_msg_parse, "Fs::Parse");
            HANDLE_CLIENT_IPC(delete_entry, epoc::fs_msg_delete, "Fs::Delete");
            HANDLE_CLIENT_IPC(set_entry, epoc::fs_msg_set_entry, "Fs::SetEntry");
            HANDLE_CLIENT_IPC(rename, epoc::fs_msg_rename, "Fs::Rename(Move)");
            HANDLE_CLIENT_IPC(replace, epoc::fs_msg_replace, "Fs::Replace");
            HANDLE_CLIENT_IPC(read_file_section, epoc::fs_msg_read_file_section, "Fs::ReadFileSection");
            HANDLE_CLIENT_IPC(set_should_notify_failure, epoc::fs_msg_set_notify_user, "Fs::SetShouldNotifyFailure");
            HANDLE_CLIENT_IPC(drive_list, epoc::fs_msg_drive_list, "Fs::DriveList");
            HANDLE_CLIENT_IPC(drive, epoc::fs_msg_drive, "Fs::Drive");
            HANDLE_CLIENT_IPC(server<fs_server>()->synchronize_driver, epoc::fs_msg_sync_drive_thread, "Fs::SyncDriveThread");
            HANDLE_CLIENT_IPC(server<fs_server>()->private_path, epoc::fs_msg_private_path, "Fs::PrivatePath");
            HANDLE_CLIENT_IPC(volume, epoc::fs_msg_volume, "Fs::Volume");
            HANDLE_CLIENT_IPC(query_drive_info_ext, epoc::fs_msg_query_volume_info_ext, "Fs::QueryVolumeInfoExt");
            HANDLE_CLIENT_IPC(server<fs_server>()->set_default_system_path, epoc::fs_msg_set_default_path, "Fs::SetDefaultPath");
            HANDLE_CLIENT_IPC(server<fs_server>()->get_default_system_path, epoc::fs_msg_default_path, "Fs::DefaultPath");
            HANDLE_CLIENT_IPC(is_file_opened, epoc::fs_msg_is_file_open, "Fs::IsFileOpen");

        case epoc::fs_msg_base_close:
            if (ctx->sys->get_symbian_version_use() < epocver::eka2) {
                generic_close(ctx);
            }

            break;

        default: {
            LOG_ERROR(SERVICE_EFSRV, "Unknown FSServer client opcode {}!", ctx->msg->function);
            break;
        }

#undef HANDLE_CLIENT_IPC
        }
    }

    void fs_server_client::replace(service::ipc_context *ctx) {
        auto given_path_target = ctx->get_argument_value<std::u16string>(0);
        auto given_path_dest = ctx->get_argument_value<std::u16string>(1);

        if (!given_path_target || !given_path_dest) {
            ctx->complete(epoc::error_argument);
            return;
        }

        auto target = get_full_symbian_path(ss_path, given_path_target.value());
        auto dest = get_full_symbian_path(ss_path, given_path_dest.value());

        io_system *io = ctx->sys->get_io_system();

        // If exists, delete it so the new file can be replaced
        if (io->exist(dest)) {
            io->delete_entry(dest);
        }

        bool res = io->rename(target, dest);

        if (!res) {
            ctx->complete(epoc::error_general);
            return;
        }

        // A new app list may be created
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::rename(service::ipc_context *ctx) {
        auto given_path_target = ctx->get_argument_value<std::u16string>(0);
        auto given_path_dest = ctx->get_argument_value<std::u16string>(1);

        if (!given_path_target || !given_path_dest) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::u16string target = get_full_symbian_path(ss_path, given_path_target.value());
        std::u16string dest = get_full_symbian_path(ss_path, given_path_dest.value());

        io_system *io = ctx->sys->get_io_system();

        if (!io->exist(target)) {
            if (eka2l1::is_separator(target.back())) {
                // Want to rename a directory. It's path not found in this case
                ctx->complete(epoc::error_path_not_found);
            } else {
                ctx->complete(epoc::error_not_found);
            }

            return;
        }

        if (io->exist(dest)) {
            ctx->complete(epoc::error_already_exists);
            return;
        }

        bool res = io->rename(target, dest);

        if (!res) {
            ctx->complete(epoc::error_general);
            return;
        }

        // A new app list may be created
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::delete_entry(service::ipc_context *ctx) {
        auto given_path = ctx->get_argument_value<std::u16string>(0);

        if (!given_path) {
            ctx->complete(epoc::error_argument);
            return;
        }

        auto path = get_full_symbian_path(ss_path, given_path.value());
        if (!check_path_capabilities_pass(path, ctx->msg->own_thr->owning_process(), epoc::fs::private_comp_access_policy, epoc::fs::sys_resource_modify_access_policy, epoc::fs::sys_resource_modify_access_policy)) {
            ctx->complete(epoc::error_permission_denied);
            return;
        }

        io_system *io = ctx->sys->get_io_system();

        bool success = io->delete_entry(path);

        if (!success) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void fs_server::synchronize_driver(service::ipc_context *ctx) {
        ctx->complete(epoc::error_none);
    }

    void fs_server::set_default_system_path(service::ipc_context *ctx) {
        auto new_path = ctx->get_argument_value<std::u16string>(0);

        if (!new_path) {
            ctx->complete(epoc::error_argument);
            return;
        }

        default_sys_path = std::move(new_path.value());
        ctx->complete(epoc::error_none);
    }

    void fs_server::get_default_system_path(service::ipc_context *ctx) {
        ctx->write_arg(0, default_sys_path);
        ctx->complete(epoc::error_none);
    }

    bool fs_server::is_file_opened(const std::u16string &path) {
        for (auto &[path_attrib, attrib] : attribs) {
            if (common::compare_ignore_case(path_attrib, path) == 0) {
                return true;
            }
        }

        return false;
    }

    void fs_server::init() {
        if (flags & FLAG_INITED) {
            return;
        }

        if (kern->is_eka1()) {
            // Create predictable directories if it does not exist
            std::u16string system_apps_dir = u"?:\\System\\Apps\\";
            std::u16string shared_data_dir = u"?:\\System\\SharedData\\";
            io_system *io = sys->get_io_system();

            // Ignore drive z.
            for (drive_number drv = drive_y; drv >= drive_a; drv--) {
                if (io->get_drive_entry(drv)) {
                    system_apps_dir[0] = drive_to_char16(drv);
                    shared_data_dir[0] = drive_to_char16(drv);

                    io->create_directories(system_apps_dir);
                    io->create_directories(shared_data_dir);
                }
            }
        }

        flags |= FLAG_INITED;
    }

    void fs_server::connect(service::ipc_context &ctx) {
        if (!(flags & FLAG_INITED)) {
            init();
        }

        fs_server_client *cli = create_session<fs_server_client>(&ctx, &ctx);
        typical_server::connect(ctx);
    }

    void fs_server::disconnect(service::ipc_context &ctx) {
        typical_server::disconnect(ctx);
    }

    void fs_server_client::session_path(service::ipc_context *ctx) {
        ctx->write_arg(0, ss_path);
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::set_session_path(service::ipc_context *ctx) {
        auto new_path = ctx->get_argument_value<std::u16string>(0);

        if (!new_path) {
            ctx->complete(epoc::error_argument);
            return;
        }

        ss_path = std::move(new_path.value());
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::set_session_to_private(service::ipc_context *ctx) {
        auto drive_ordinal = ctx->get_argument_value<std::int32_t>(0);

        if (!drive_ordinal) {
            ctx->complete(epoc::error_argument);
            return;
        }

        ss_path = get_private_path(ctx->msg->own_thr->owning_process(), static_cast<drive_number>(drive_ordinal.value()));
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::create_private_path(service::ipc_context *ctx) {
        const std::u16string private_path = get_private_path(ctx->msg->own_thr->owning_process(),
            static_cast<drive_number>(ctx->get_argument_value<std::int32_t>(0).value()));

        eka2l1::io_system *io = ctx->sys->get_io_system();

        // What you want more? The doc does not say anything, and app wants this
        // Example: n-gage 2.0
        if (io->exist(private_path)) {
            ctx->complete(epoc::error_none);
            return;
        }

        if (!io->create_directory(private_path)) {
            ctx->complete(epoc::error_permission_denied);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void fs_server_client::is_file_in_rom(service::ipc_context *ctx) {
        std::optional<utf16_str> path = ctx->get_argument_value<utf16_str>(0);

        if (!path) {
            ctx->complete(epoc::error_argument);
            return;
        }

        auto final_path = get_full_symbian_path(ss_path, path.value());
        symfile f = ctx->sys->get_io_system()->open_file(final_path, READ_MODE);

        if (!f) {
            ctx->complete(0);
            return;
        }

        address addr = f->rom_address();

        f->close();

        ctx->write_data_to_descriptor_argument<address>(1, addr);
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::is_valid_name(service::ipc_context *ctx) {
        std::optional<utf16_str> path = ctx->get_argument_value<utf16_str>(0);

        if (!path) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::regex pattern("[<>:\"/|*?]+");
        std::string path_utf8 = common::ucs2_to_utf8(path.value());
        std::uint32_t valid = !(std::regex_match(path_utf8, pattern) || path->empty());

        ctx->write_data_to_descriptor_argument<std::uint32_t>(1, valid);
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::notify_change(service::ipc_context *ctx) {
        kernel_system *kern = ctx->sys->get_kernel_system();
        notify_entry entry;

        entry.match_pattern = ".*";
        entry.type = static_cast<notify_type>(*ctx->get_argument_value<std::int32_t>(0));
        entry.info = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);

        if (kern->is_eka1()) {
            std::optional<address> notify_reqaddr = ctx->get_argument_value<address>(1);
            if (!notify_reqaddr.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            entry.info.sts = notify_reqaddr.value();
            ctx->complete(epoc::error_none);
        }

        notify_entries.push_back(entry);
    }

    void fs_server_client::notify_change_ex(service::ipc_context *ctx) {
        kernel_system *kern = ctx->sys->get_kernel_system();
        std::optional<utf16_str> wildcard_match = ctx->get_argument_value<utf16_str>(1);

        if (!wildcard_match) {
            ctx->complete(epoc::error_argument);
            return;
        }

        notify_entry entry;
        entry.match_pattern = common::wildcard_to_regex_string(common::ucs2_to_utf8(*wildcard_match));
        entry.type = static_cast<notify_type>(*ctx->get_argument_value<std::int32_t>(0));
        entry.info = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);

        if (kern->is_eka1()) {
            std::optional<address> notify_reqaddr = ctx->get_argument_value<address>(2);
            if (!notify_reqaddr.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            entry.info.sts = notify_reqaddr.value();
            ctx->complete(epoc::error_none);
        }

        notify_entries.push_back(entry);

        LOG_TRACE(SERVICE_EFSRV, "Notify requested with wildcard: {}", common::ucs2_to_utf8(*wildcard_match));
    }

    void fs_server_client::notify_change_cancel(service::ipc_context *ctx) {
        for (auto it = notify_entries.begin(); it != notify_entries.end(); ++it) {
            it->info.complete(epoc::error_cancel);
        }

        notify_entries.clear();
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::notify_change_cancel_ex(service::ipc_context *ctx) {
        address request_status_addr = ctx->get_argument_value<address>(0).value();
        for (auto it = notify_entries.begin(); it != notify_entries.end(); ++it) {
            notify_entry entry = *it;
            if (entry.info.sts.ptr_address() == request_status_addr) {
                entry.info.complete(epoc::error_cancel);
                notify_entries.erase(it);
                break;
            }
        }
        ctx->complete(epoc::error_none);
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
        std::optional<std::u16string> dir = ctx->get_argument_value<std::u16string>(0);

        if (!dir) {
            ctx->complete(epoc::error_argument);
            return;
        }

        dir.value() = eka2l1::absolute_path(dir.value(), ss_path, true);

        if (!check_path_capabilities_pass(dir.value(), ctx->msg->own_thr->owning_process(), epoc::fs::private_comp_access_policy, epoc::fs::sys_resource_modify_access_policy, epoc::fs::sys_resource_modify_access_policy)) {
            ctx->complete(epoc::error_permission_denied);
            return;
        }

        io_system *io = ctx->sys->get_io_system();

        bool res = false;

        if (!io->exist(dir.value())) {
            if (*ctx->get_argument_value<std::int32_t>(1)) {
                res = io->create_directories(eka2l1::file_directory(*dir));
            } else {
                res = io->create_directory(eka2l1::file_directory(*dir));
            }
        }

        if (!res) {
            // The guide specified: if it's parent does not exist or the sub-directory
            // already created, this should returns
            ctx->complete(epoc::error_already_exists);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void fs_server_client::rmdir(service::ipc_context *ctx) {
        std::optional<std::u16string> dir = ctx->get_argument_value<std::u16string>(0);

        if (!dir) {
            ctx->complete(epoc::error_argument);
            return;
        }

        io_system *io = ctx->sys->get_io_system();
        io->delete_entry(dir.value());

        ctx->complete(epoc::error_none);
    }

    void fs_server_client::parse(service::ipc_context *ctx) {
        auto name = ctx->get_argument_value<std::u16string>(0);
        auto related = ctx->get_argument_value<std::u16string>(1);
        auto parse = ctx->get_argument_data_from_descriptor<file_parse>(2);

        if (!name || !parse) {
            ctx->complete(epoc::error_argument);
            return;
        }

        file_parser parser(name.value(), related.value_or(u""), parse.value());
        parser.parse(server<fs_server>()->default_sys_path);

        ctx->write_data_to_descriptor_argument(2, parser.get_result());
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::entry(service::ipc_context *ctx) {
        std::optional<std::u16string> fname_op = ctx->get_argument_value<std::u16string>(0);

        if (!fname_op) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::u16string fname = std::move(*fname_op);
        fname = get_full_symbian_path(ss_path, fname);

        if (!check_path_capabilities_pass(fname, ctx->msg->own_thr->owning_process(), epoc::fs::private_comp_access_policy, epoc::fs::sys_read_only_access_policy, epoc::fs::resource_read_only_access_policy)) {
            ctx->complete(epoc::error_permission_denied);
            return;
        }

        LOG_INFO(SERVICE_EFSRV, "Get entry of: {}", common::ucs2_to_utf8(fname));

        bool dir = false;

        io_system *io = ctx->sys->get_io_system();

        std::optional<entry_info> entry_hle = io->get_entry_info(fname);

        if (!entry_hle) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        epoc::fs::entry entry;
        epoc::fs::build_symbian_entry_from_emulator_entry(io, entry_hle.value(), entry);

        ctx->write_data_to_descriptor_argument<epoc::fs::entry>(1, entry, nullptr, true);
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::set_entry(service::ipc_context *ctx) {
        std::optional<std::u16string> fname_op = ctx->get_argument_value<std::u16string>(0);
        std::optional<epoc::time> time = ctx->get_argument_data_from_descriptor<epoc::time>(1);
        std::uint32_t set_att_mask = *ctx->get_argument_value<std::uint32_t>(2);
        std::uint32_t clear_att_mask = *ctx->get_argument_value<std::uint32_t>(3);

        if (!fname_op) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const std::u16string fname = get_full_symbian_path(ss_path, fname_op.value());
        LOG_INFO(SERVICE_EFSRV, "Set entry of: {}", common::ucs2_to_utf8(fname));

        io_system *io = ctx->sys->get_io_system();

        std::optional<entry_info> entry_hle = io->get_entry_info(fname);

        if (!entry_hle) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void fs_server::private_path(service::ipc_context *ctx) {
        std::u16string path = get_private_path_trim_uid(ctx->msg->own_thr->owning_process());

        ctx->write_arg(0, path);
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::set_should_notify_failure(service::ipc_context *ctx) {
        should_notify_failures = static_cast<bool>(ctx->get_argument_value<std::int32_t>(0));
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::generic_close(service::ipc_context *ctx) {
        std::optional<std::int32_t> handle_res = ctx->get_argument_value<std::int32_t>(3);

        if (!handle_res) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);

        switch (node->vfs_node->type) {
        case io_component_type::file: {
            file_close(ctx);
            break;
        }

        case io_component_type::dir:
            close_dir(ctx);
            break;

        default:
            LOG_WARN(SERVICE_EFSRV, "Unhandled close type for node type {}", static_cast<int>(node->vfs_node->type));
            break;
        }
    }

    void fs_server_client::is_file_opened(service::ipc_context *ctx) {
        std::optional<utf16_str> path = ctx->get_argument_value<utf16_str>(0);

        if (!path) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::u16string final_path = get_full_symbian_path(ss_path, path.value());
        const std::int32_t result = server<fs_server>()->is_file_opened(final_path);

        ctx->write_data_to_descriptor_argument<std::int32_t>(1, result);
        ctx->complete(epoc::error_none);
    }
}
