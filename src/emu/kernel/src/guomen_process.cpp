/*
 * Copyright (c) 2023 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <kernel/guomen_process.h>
#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <utils/apacmd.h>
#include <utils/reqsts.h>
#include <utils/err.h>

#include <common/log.h>

namespace eka2l1::kernel {
    guomen_process::guomen_process(kernel_system *kern, memory_system *mem, const std::u16string &cmd_args)
        : process(kern, mem, "Door crossing process", kernel::BRIDAGED_EXECUTABLE_NAME, cmd_args, true) {
        // Create a stub thread with minimal stack and heap size
        primary_thread
            = kern->create<kernel::thread>(
                mem,
                kern->get_ntimer(),
                this,
                kernel::access_type::local_access,
                "Main", 0,
                0x1000, 0, 0x1000,
                true,
                0, 0, kernel::priority_normal);

        // If this thread dies, processes booms
        primary_thread->set_process_permanent(true);
    }

    bool guomen_process::run() {
        if (ran_requested) {
            return true;
        }

        if (!get_kernel_object_owner()->handle_guomen_process_run(this)) {
            return false;
        }

        rendezvous(epoc::error_none);
        ran_requested = true;

        return true;
    }

    bool guomen_process::handle_rendezvous_request(epoc::notify_info &info) {
        if (!ran_requested) {
            return false;
        }

        info.complete(epoc::error_none);
        return true;
    }
}