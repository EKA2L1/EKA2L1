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

#include <epoc/services/loader/loader.h>
#include <epoc/services/loader/op.h>

#include <epoc/kernel.h>
#include <epoc/utils/des.h>

#include <epoc/epoc.h>
#include <epoc/vfs.h>

#include <epoc/loader/romimage.h>

#include <common/cvt.h>
#include <common/e32inc.h>
#include <common/log.h>
#include <common/path.h>

#include <epoc/kernel/libmanager.h>

#include <common/algorithm.h>

#include <cwctype>
#include <e32err.h>

namespace eka2l1 {
    void loader_server::load_process(eka2l1::service::ipc_context &ctx) {
        std::optional<epoc::ldr_info> info = ctx.get_arg_packed<epoc::ldr_info>(0);

        if (!info) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        std::optional<std::u16string> process_name16 = ctx.get_arg<std::u16string>(1);
        std::optional<std::u16string> process_args = ctx.get_arg<std::u16string>(2);

        if (!process_name16 || !process_args) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        std::string name_process = eka2l1::filename(common::ucs2_to_utf8(*process_name16));

        LOG_TRACE("Trying to summon: {}", name_process);

        std::u16string pext = path_extension(*process_name16);

        if (pext.empty()) {
            // Just append it
            *process_name16 += u".exe";
        }

        kernel_system *kern = ctx.sys->get_kernel_system();

        process_ptr pr = kern->spawn_new_process(*process_name16,
            *process_args, info->uid3, info->min_stack_size);

        if (!pr) {
            ctx.set_request_status(KErrNotFound);
            return;
        }

        info->handle = kern->open_handle_with_thread(ctx.msg->own_thr, pr,
            static_cast<kernel::owner_type>(info->owner_type));

        ctx.write_arg_pkg(0, *info);
        ctx.set_request_status(KErrNone);

        // 4201636
        void *me = (ctx.sys->get_memory_system()->get_real_pointer(4201636));
        int b = 5;
    }

    void loader_server::load_library(service::ipc_context &ctx) {
        std::optional<epoc::ldr_info> info = ctx.get_arg_packed<epoc::ldr_info>(0);

        if (!info) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        std::optional<std::u16string> lib_path = ctx.get_arg<std::u16string>(1);

        if (!lib_path) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        std::string lib_name = eka2l1::filename(common::ucs2_to_utf8(*lib_path));
        std::u16string pext = path_extension(*lib_path);

        // This hack prevents the lib manager from loading wrong file.
        // For example, if there is no extension, and the file is load must be DLL, and two files with same name
        // Wrong file might be loaded.
        if (pext.empty()) {
            // Just append it
            *lib_path += u".dll";
        }

        codeseg_ptr cs = ctx.sys->get_lib_manager()->load(*lib_path);

        if (!cs) {
            ctx.set_request_status(KErrNotFound);
            return;
        }

        /* Create process through kernel system. */
        kernel::handle lib_handle = ctx.sys->get_kernel_system()->create_and_add_thread<kernel::library>(
                                                                    static_cast<kernel::owner_type>(info->owner_type), ctx.msg->own_thr, cs)
                                        .first;

        LOG_TRACE("Loaded library: {}", lib_name);

        info->handle = lib_handle;

        ctx.msg->own_thr->owning_process()->signal_dll_lock(ctx.msg->own_thr);

        ctx.write_arg_pkg(0, *info);
        ctx.set_request_status(KErrNone);
    }

    void loader_server::get_info(service::ipc_context &ctx) {
        std::optional<utf16_str> lib_name = ctx.get_arg<utf16_str>(1);
        epoc::des8 *buffer = eka2l1::ptr<epoc::des8>(ctx.msg->args.args[2]).get(ctx.sys->get_kernel_system()->crr_process());

        if (!lib_name || !buffer) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        // Check the buffer size
        if (buffer->max_length < sizeof(epoc::lib_info)) {
            ctx.set_request_status(KErrNoMemory);
            return;
        }

        epoc::ldr_info load_info;
        epoc::lib_info linfo;

        if (!epoc::get_image_info(ctx.sys->get_lib_manager(), *lib_name, linfo)) {
            ctx.set_request_status(KErrNotFound);
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

            ctx.write_arg_pkg(2, reinterpret_cast<uint8_t *>(&linfo2), sizeof(epoc::lib_info2));
        } else {
            ctx.write_arg_pkg(2, reinterpret_cast<uint8_t *>(&linfo), sizeof(epoc::lib_info));
        }

        ctx.write_arg_pkg<epoc::ldr_info>(0, load_info);
        ctx.set_request_status(KErrNone);
    }

    loader_server::loader_server(system *sys)
        : service::server(sys, "!Loader", true) {
        REGISTER_IPC(loader_server, load_process, ELoadProcess, "Loader::LoadProcess");
        REGISTER_IPC(loader_server, load_library, ELoadLibrary, "Loader::LoadLibrary");
        REGISTER_IPC(loader_server, get_info, EGetInfo, "Loader::GetInfo");
    }
}
