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

#include <core/core_kernel.h>
#include <core/core_mem.h>

#include <core/kernel/process.h>
#include <core/kernel/scheduler.h>

#include <core/loader/romimage.h>

namespace eka2l1::kernel {
    void process::create_prim_thread(uint32_t code_addr, uint32_t ep_off, uint32_t stack_size, uint32_t heap_min,
        uint32_t heap_max) {
        // Preserve the page table
        page_table *tab = mem->get_current_page_table();
        mem->set_current_page_table(page_tab);

        primary_thread = kern->create_thread(kernel::owner_type::kernel, nullptr, kernel::access_type::local_access,
            process_name, ep_off,
            stack_size, heap_min, heap_max,
            0, kernel::priority_normal);

        args[0].data_size = 0;
        args[1].data_size = (5 + exe_path.size() * 2 + cmd_args.size() * 2); // Contains some garbage :D

        if (tab)
            mem->set_current_page_table(*tab);
    }

    process::process(kernel_system *kern, memory_system *mem, uint32_t uid,
        const std::string &process_name, const std::u16string &exe_path,
        const std::u16string &cmd_args, loader::e32img_ptr &img)
        : kernel_obj(kern, process_name, access_type::local_access)
        , uid(uid)
        , process_name(process_name)
        , kern(kern)
        , mem(mem)
        , img(img)
        , exe_path(exe_path)
        , cmd_args(cmd_args)
        , page_tab(mem->get_page_size())
        , process_handles(kern, handle_array_owner::process) {
        obj_type = kernel::object_type::process;

        // Preserve the page table
        create_prim_thread(img->rt_code_addr, img->rt_code_addr + img->header.entry_point, img->header.stack_size,
            img->header.heap_size_min, img->header.heap_size_max);
    }

    process::process(kernel_system *kern, memory_system *mem, uint32_t uid,
        const std::string &process_name, const std::u16string &exe_path,
        const std::u16string &cmd_args, loader::romimg_ptr &img)
        : kernel_obj(kern, process_name, access_type::local_access)
        , uid(uid)
        , process_name(process_name)
        , kern(kern)
        , mem(mem)
        , romimg(img)
        , exe_path(exe_path)
        , cmd_args(cmd_args)
        , page_tab(mem->get_page_size())
        , process_handles(kern, handle_array_owner::process) {
        obj_type = kernel::object_type::process;

        // Preserve the page table
        page_table *tab = mem->get_current_page_table();
        mem->set_current_page_table(page_tab);

        create_prim_thread(
            romimg->header.code_address, romimg->header.entry_point,
            romimg->header.stack_size, romimg->header.heap_minimum_size, romimg->header.heap_maximum_size);
    }

    void process::set_arg_slot(uint8_t slot, uint32_t data, size_t data_size) {
        if (slot >= 16) {
            return;
        }

        args[slot].data = data;
        args[slot].data_size = data_size;
    }

    std::optional<pass_arg> process::get_arg_slot(uint8_t slot) {
        if (slot >= 16) {
            return std::optional<pass_arg>{};
        }

        return args[slot];
    }

    process_uid_type process::get_uid_type() {
        return std::tuple(0x1000007A, 0x100039CE, uid);
    }

    kernel_obj_ptr process::get_object(uint32_t handle) {
        return process_handles.get_object(handle);
    }

    bool process::run() {
        thread_ptr thr = kern->get_thread_by_handle(primary_thread);

        if (!thr) {
            return false;
        }

        thr->owning_process(kern->get_process(obj_name));

        kern->run_thread(primary_thread);

        return true;
    }
}
