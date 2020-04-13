/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#pragma once

#include <memory>

namespace eka2l1 {
    namespace kernel {
        class thread;
    }
}

namespace eka2l1::scripting {
    class process;

    class thread {
        eka2l1::kernel::thread *thread_handle;

        friend class eka2l1::kernel::thread;

    public:
        thread(uint64_t handle);

        std::string get_name();

        uint32_t get_register(uint8_t index);

        uint32_t get_pc();
        uint32_t get_lr();
        uint32_t get_sp();
        uint32_t get_cpsr();

        int get_exit_reason();
        int get_leave_depth();

        int get_state();
        int get_priority();

        std::uint32_t get_stack_base();
        std::uint32_t get_heap_base();

        std::unique_ptr<scripting::process> get_owning_process();
    };

    std::unique_ptr<eka2l1::scripting::thread> get_current_thread();
}
