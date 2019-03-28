/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
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
#include <vector>

namespace eka2l1 {
    namespace kernel {
        class process;
    }
}

namespace eka2l1::scripting {
    class thread;

    class process {
        eka2l1::kernel::process *process_handle;

    public:
        process(uint64_t handle);

        bool read_process_memory(const std::uint32_t addr, std::vector<char> &buffer, const size_t size);
        bool write_process_memory(const std::uint32_t addr, std::vector<char> buffer);

        std::string get_executable_path();
        std::string get_name();

        std::vector<std::unique_ptr<eka2l1::scripting::thread>> get_thread_list();
    };

    std::vector<std::unique_ptr<eka2l1::scripting::process>> get_process_list();
    std::unique_ptr<eka2l1::scripting::process> get_current_process();
}
