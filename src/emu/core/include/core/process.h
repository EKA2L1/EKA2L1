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
#pragma once

#include <kernel/thread.h>
#include <loader/eka2img.h>
#include <string>

namespace eka2l1 {
    class kernel_system;
    class memory;

    struct process_info {
        ptr<void> code_where;
        uint64_t size;
    };

    namespace loader {
        using e32img_ptr = std::shared_ptr<eka2img>;
    }

    using thread_ptr = std::shared_ptr<kernel::thread>;

    struct pass_arg {
        uint32_t data = 0;
        int data_size = -1;
    };

    class process {
        uint32_t uid;
        std::string process_name;
        thread_ptr prthr;

        loader::e32img_ptr img;

        kernel_system *kern;
        memory_system *mem;

        std::vector<thread_ptr> own_threads;
        std::array<pass_arg, 16> args;
        
        std::u16string exe_path;
        std::u16string cmd_args;

    public:
        process() = default;
        process(kernel_system *kern, memory_system *mem, uint32_t uid,
            const std::string &process_name, const std::u16string &exe_path, const std::u16string &cmd_args, loader::e32img_ptr &img);

        std::u16string get_cmd_args() const {
            return cmd_args;
        }

        std::u16string get_exe_path() const {
            return exe_path;
        }

        uint32_t get_uid() {
            return uid;
        }

        loader::e32img_ptr get_e32img() {
            return img;
        }

        void set_arg_slot(uint8_t slot, uint32_t data, size_t data_size);
        std::optional<pass_arg> get_arg_slot(uint8_t slot);

        std::string name() const {
            return process_name;
        }

        // Create a new thread and run
        // No arguments provided
        bool run();

        // Step through instructions
        bool step();

        // LOL
        bool suspend() { return true; }

        // Stop the main process thread
        bool stop();
    };
}

