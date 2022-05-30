/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <services/context.h>
#include <services/loader/loader.h>
#include <services/loader/op.h>

#include <kernel/kernel.h>
#include <utils/des.h>

#include <system/epoc.h>
#include <vfs/vfs.h>

#include <loader/romimage.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/pystr.h>

#include <kernel/libmanager.h>

#include <common/algorithm.h>

#include <cwctype>
#include <utils/err.h>

namespace eka2l1 {
    const std::string get_loader_server_name_through_epocver(const epocver ver) {
        if (ver < epocver::eka2) {
            return "Loader";
        }

        return "!Loader";
    }

    static std::vector<common::pystr16> get_additional_search_paths(const std::u16string &search_list) {
        common::pystr16 str(search_list);
        return str.split(u';');
    }

    void loader_server::load_process(eka2l1::service::ipc_context &ctx) {
        std::optional<epoc::ldr_info> info = std::nullopt;
        std::optional<epoc::ldr_info_eka1> info_eka1 = std::nullopt;

        epoc::owner_type handle_owner = epoc::owner_thread;
        std::optional<std::u16string> process_name16 = std::nullopt;
        std::optional<std::u16string> process_args = std::nullopt;

        std::uint32_t stack_size = 0;
        std::uint32_t uid3 = 0;

        if (kern->is_eka1()) {
            info_eka1 = ctx.get_argument_data_from_descriptor<epoc::ldr_info_eka1>(0);

            if (!info_eka1) {
                ctx.complete(epoc::error_argument);
                return;
            }

            process_name16 = info_eka1->full_path_.to_std_string(nullptr);
            process_args = ctx.get_argument_value<std::u16string>(1);

            uid3 = info_eka1->uid3_;
            handle_owner = info_eka1->handle_owner_;
            stack_size = 0; // Eka1 does not allow custom stack size
        } else {
            info = ctx.get_argument_data_from_descriptor<epoc::ldr_info>(0);
            process_name16 = ctx.get_argument_value<std::u16string>(1);
            process_args = ctx.get_argument_value<std::u16string>(2);

            if (!info) {
                ctx.complete(epoc::error_argument);
                return;
            }

            handle_owner = info->owner_type;
            uid3 = info->uid3;
            stack_size = info->min_stack_size;
        }

        if (!process_name16 || !process_args) {
            ctx.complete(epoc::error_argument);
            return;
        }

        const std::string name_process = eka2l1::filename(common::ucs2_to_utf8(*process_name16));

        LOG_TRACE(SERVICE_LOADER, "Trying to summon: {}", name_process);

        std::u16string pext = path_extension(*process_name16);

        if (pext.empty()) {
            // Just append it
            *process_name16 += u".exe";
        }

        kernel_system *kern = ctx.sys->get_kernel_system();
        process_ptr pr = kern->spawn_new_process(*process_name16, *process_args, uid3, stack_size);

        if (!pr) {
            LOG_DEBUG(SERVICE_LOADER, "Try spawning process {} failed", name_process);
            ctx.complete(epoc::error_not_found);
            return;
        }

        process_ptr request_pr = ctx.msg->own_thr->owning_process();
        if (request_pr) {
            request_pr->add_child_process(pr);
        }

        kernel::handle pr_handle = kern->open_handle_with_thread(ctx.msg->own_thr, pr, static_cast<kernel::owner_type>(handle_owner));

        if (info) {
            info->handle = pr_handle;
            ctx.write_data_to_descriptor_argument(0, *info);
        } else {
            info_eka1->result_handle = pr_handle;
            ctx.write_data_to_descriptor_argument(0, *info_eka1);
        }

        ctx.complete(epoc::error_none);
    }

    void loader_server::load_library(service::ipc_context &ctx) {
        std::optional<epoc::ldr_info> info = std::nullopt;
        std::optional<epoc::ldr_info_eka1> info_eka1 = std::nullopt;

        epoc::owner_type handle_owner = epoc::owner_thread;
        std::optional<std::u16string> lib_path = std::nullopt;

        if (kern->is_eka1()) {
            info_eka1 = ctx.get_argument_data_from_descriptor<epoc::ldr_info_eka1>(0);

            if (!info_eka1) {
                ctx.complete(epoc::error_argument);
                return;
            }

            lib_path = info_eka1->full_path_.to_std_string(nullptr);
        } else {
            info = ctx.get_argument_data_from_descriptor<epoc::ldr_info>(0);

            if (!info) {
                ctx.complete(epoc::error_argument);
                return;
            }

            handle_owner = info->owner_type;
            lib_path = ctx.get_argument_value<std::u16string>(1);
        }

        if (!lib_path) {
            ctx.complete(epoc::error_argument);
            return;
        }

        const std::string lib_name = eka2l1::filename(common::ucs2_to_utf8(*lib_path));
        const std::u16string pext = path_extension(*lib_path);

        // This hack prevents the lib manager from loading wrong file.
        // For example, if there is no extension, and the file is load must be DLL, and two files with same name
        // Wrong file might be loaded.
        if (pext.empty()) {
            // Just append it
            *lib_path += u".dll";
        }

        // Access to this library manager is locked by kernel lock, so we directly append additional search path
        hle::lib_manager *mngr = ctx.sys->get_lib_manager();
        kernel::process *own_pr = ctx.msg->own_thr->owning_process();

        std::vector<common::pystr16> search_list;

        if (info_eka1) {
            std::u16string search_list_str = info_eka1->search_path_.to_std_string(own_pr);
            search_list = get_additional_search_paths(search_list_str);
        }

        for (auto &search_path : search_list) {
            mngr->search_paths.insert(mngr->search_paths.begin(), search_path.std_str());
        }

        codeseg_ptr cs = mngr->load(*lib_path);

        if (!search_list.empty()) {
            mngr->search_paths.erase(mngr->search_paths.begin(), mngr->search_paths.begin() + search_list.size());
        }

        if (!cs) {
            LOG_DEBUG(SERVICE_LOADER, "Try loading {} to {} failed", lib_name, own_pr->name());
            ctx.complete(epoc::error_not_found);
            return;
        }

        /* Create process through kernel system. */
        auto lib_handle_and_obj = ctx.sys->get_kernel_system()->create_and_add_thread<kernel::library>(
            static_cast<kernel::owner_type>(handle_owner), ctx.msg->own_thr, cs);

        LOG_TRACE(SERVICE_LOADER, "Loaded library: {}", lib_name);

        if (info) {
            own_pr->signal_dll_lock(ctx.msg->own_thr);

            info->handle = lib_handle_and_obj.first;
            ctx.write_data_to_descriptor_argument(0, *info);
        } else {
            // Don't know what they are but they got involved in Payload crash (uninitialized variable that should be 1 through this function)
            info_eka1->unkXX[0] = 1;
            info_eka1->unkXX[1] = 1;
            info_eka1->unkXX[2] = 1;

            info_eka1->result_handle = lib_handle_and_obj.first;
            ctx.write_data_to_descriptor_argument(0, *info_eka1);
        }

        ctx.complete(epoc::error_none);
    }

    void loader_server::get_info(service::ipc_context &ctx) {
        std::optional<utf16_str> lib_name = ctx.get_argument_value<utf16_str>(1);
        epoc::des8 *buffer = eka2l1::ptr<epoc::des8>(ctx.msg->args.args[2]).get(ctx.sys->get_kernel_system()->crr_process());

        if (!lib_name || !buffer) {
            ctx.complete(epoc::error_argument);
            return;
        }

        // Check the buffer size
        if (buffer->max_length < sizeof(epoc::lib_info)) {
            ctx.complete(epoc::error_no_memory);
            return;
        }

        epoc::ldr_info load_info;
        epoc::lib_info linfo;

        if (!epoc::get_image_info(ctx.sys->get_lib_manager(), *lib_name, linfo)) {
            ctx.complete(epoc::error_not_found);
            return;
        }

        load_info.uid1 = linfo.uid1;
        load_info.uid2 = linfo.uid2;
        load_info.uid3 = linfo.uid3;
        load_info.secure_id = linfo.secure_id;

        if (buffer->max_length >= sizeof(epoc::lib_info2)) {
            epoc::lib_info2 linfo2;
            memcpy(&linfo2, &linfo, sizeof(epoc::lib_info));

            linfo2.debug_attrib = 1;
            linfo2.hfp = 0;

            ctx.write_data_to_descriptor_argument(2, reinterpret_cast<uint8_t *>(&linfo2), sizeof(epoc::lib_info2));
        } else {
            ctx.write_data_to_descriptor_argument(2, reinterpret_cast<uint8_t *>(&linfo), sizeof(epoc::lib_info));
        }

        ctx.write_data_to_descriptor_argument<epoc::ldr_info>(0, load_info);
        ctx.complete(epoc::error_none);
    }

    void loader_server::get_info_from_header(service::ipc_context &context) {
        std::uint8_t *header_data = context.get_descriptor_argument_ptr(0);
        const std::size_t header_size = context.get_argument_data_size(0);

        if (!header_data || !header_size) {
            context.complete(epoc::error_argument);
            return;
        }

        epoc::lib_info linfo;
        common::ro_buf_stream header_stream(header_data, header_size);

        const std::int32_t err = epoc::get_image_info_from_stream(reinterpret_cast<common::ro_stream *>(&header_stream), linfo);

        if (err != epoc::error_none) {
            context.complete(err);
            return;
        }

        const std::size_t avail_size_write = context.get_argument_max_data_size(1);

        if (avail_size_write < sizeof(epoc::lib_info)) {
            context.complete(epoc::error_argument);
            return;
        }

        if (avail_size_write >= sizeof(epoc::lib_info2)) {
            epoc::lib_info2 linfo2;
            memcpy(&linfo2, &linfo, sizeof(epoc::lib_info));

            linfo2.debug_attrib = 1;
            linfo2.hfp = 0;

            context.write_data_to_descriptor_argument(1, reinterpret_cast<uint8_t *>(&linfo2), sizeof(epoc::lib_info2));
        } else {
            context.write_data_to_descriptor_argument(1, reinterpret_cast<uint8_t *>(&linfo), sizeof(epoc::lib_info));
        }

        context.complete(epoc::error_none);
    }

    void loader_server::delete_loader(service::ipc_context &ctx) {
        std::optional<utf16_str> lib_name = ctx.get_argument_value<utf16_str>(1);

        if (!lib_name) {
            ctx.complete(epoc::error_argument);
            return;
        }

        io_system *io_sys = sys->get_io_system();
        if (!io_sys->exist(lib_name.value())) {
            ctx.complete(epoc::error_not_found);
            return;
        }
        io_sys->delete_entry(lib_name.value());

        ctx.complete(epoc::error_none);
    }

    void loader_server::check_library_hash(service::ipc_context &ctx) {
        std::optional<utf16_str> lib_name = ctx.get_argument_value<utf16_str>(1);

        if (!lib_name) {
            ctx.complete(epoc::error_argument);
            return;
        }

        ctx.complete(epoc::error_none);
    }

    void loader_server::load_logical_device(service::ipc_context &context) {
        std::optional<utf16_str> ldd_name = context.get_argument_value<utf16_str>(1);
        if (!ldd_name.has_value()) {
            context.complete(epoc::error_argument);
            return;
        }

        LOG_TRACE(SERVICE_LOADER, "Trying to load LDD {}", common::ucs2_to_utf8(ldd_name.value()));
        context.complete(epoc::error_none);
    }

    void loader_server::load_locale(service::ipc_context &context) {
        context.complete(epoc::error_not_found);
    }

    loader_server::loader_server(system *sys)
        : service::server(sys->get_kernel_system(), sys, nullptr, get_loader_server_name_through_epocver(sys->get_symbian_version_use()), true) {
        REGISTER_IPC(loader_server, load_process, ELoadProcess, "Loader::LoadProcess");
        REGISTER_IPC(loader_server, load_library, ELoadLibrary, "Loader::LoadLibrary");
        REGISTER_IPC(loader_server, get_info, EGetInfo, "Loader::GetInfo");
        REGISTER_IPC(loader_server, get_info_from_header, EGetInfoFromHeader, "Loader::GetInfoFromHeader");
        REGISTER_IPC(loader_server, delete_loader, ELdrDelete, "Loader::Delete");
        REGISTER_IPC(loader_server, check_library_hash, ECheckLibraryHash, "Loader::CheckLibraryHash");
        REGISTER_IPC(loader_server, load_locale, ELoadLocale, "Loader::LoadLocale");
        REGISTER_IPC(loader_server, load_logical_device, ELoadLogicalDevice, "Loader::LoadLogicalDevice");
    }
}
