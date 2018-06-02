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
        const std::string &process_name, loader::e32img_ptr &img)
        : uid(uid)
        , process_name(process_name)
        , kern(kern)
        , mem(mem)
        , img(img) {
        prthr = kern->add_thread(uid, process_name, img->rt_code_addr + img->header.entry_point,
            img->header.stack_size, img->header.heap_size_min, img->header.heap_size_max,
            nullptr, kernel::priority_normal);
    }

    bool process::stop() {
        prthr->stop();
        return true;
    }

    // Create a new thread and run
    // No arguments provided
    bool process::run() {
        return kern->run_thread(prthr->unique_id());
    }
}

