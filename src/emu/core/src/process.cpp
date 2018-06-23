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
#include <core_kernel.h>
#include <process.h>

namespace eka2l1 {
    process::process(kernel_system *kern, memory_system *mem, uint32_t uid,
        const std::string &process_name, const std::u16string &exe_path, const std::u16string &cmd_args, loader::e32img_ptr &img)
        : uid(uid)
        , process_name(process_name)
        , kern(kern)
        , mem(mem)
        , img(img)
        , exe_path(exe_path)
        , cmd_args(cmd_args) {
        prthr = kern->add_thread(kernel::owner_type::process, uid, kernel::access_type::local_access, process_name, img->rt_code_addr + img->header.entry_point,
            img->header.stack_size, img->header.heap_size_min, img->header.heap_size_max,
            nullptr, kernel::priority_normal);

        args[0].data_size = 0;
        args[1].data_size = (5 + exe_path.size() * 2 + cmd_args.size() * 2); // Contains some garbage :D
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

    bool process::stop() {
        prthr->stop();
        kern->close_thread(prthr->unique_id());
        return true;
    }

    // Create a new thread and run
    // No arguments provided
    bool process::run() {
        return kern->run_thread(prthr->unique_id());
    }

    process_uid_type process::get_uid_type() {
        return std::tuple(0x1000007A, 0x100039CE, uid);
    }
}

