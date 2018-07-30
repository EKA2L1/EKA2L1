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

#include <core/kernel/kernel_obj.h>
#include <core/kernel/object_ix.h>
#include <core/kernel/thread.h>

#include <core/loader/eka2img.h>

#include <core/page_table.h>

#include <string>

#include <tuple>

namespace eka2l1 {
    class kernel_system;

    namespace loader {
        using e32img_ptr = std::shared_ptr<eka2l1::loader::eka2img>;
    }

    using thread_ptr = std::shared_ptr<kernel::thread>;
}

namespace eka2l1::kernel {
    class thread_scheduler;

    struct process_info {
        ptr<void> code_where;
        uint64_t size;
    };

    struct pass_arg {
        uint32_t data = 0;
        size_t data_size = -1;
    };

    using process_uid_type = std::tuple<uint32_t, uint32_t, uint32_t>;

    enum class process_priority {
        low = 150,
        background = 250,
        foreground = 350,
        high = 450,
        window_svr = 650,
        file_svr = 750,
        real_time_svr = 850,
        supervisor = 950
    };

    class process : public kernel_obj {
        friend class kernel_system;
        friend class thread_scheduler;

        uint32_t uid, primary_thread;

        std::string process_name;

        loader::e32img_ptr img;
        loader::romimg_ptr romimg;

        kernel_system *kern;
        memory_system *mem;

        std::array<pass_arg, 16> args;

        std::u16string exe_path;
        std::u16string cmd_args;

        page_table page_tab;
        object_ix process_handles;

        uint32_t flags;

        process_priority priority;

        int rendezvous_reason = 0;
        int exit_reason = 0;

        struct logon_request_form {
            thread_ptr requester;
            int *request_status;

            explicit logon_request_form(thread_ptr thr, int *rsts)
                : requester(thr), request_status(rsts) {}
        };

        std::vector<logon_request_form> logon_requests;
        std::vector<logon_request_form> rendezvous_requests;

        uint32_t thread_count = 0;

    protected:
        void create_prim_thread(uint32_t code_addr, uint32_t ep_off, uint32_t stack_size, uint32_t heap_min,
            uint32_t heap_max);

    public:
        uint32_t increase_thread_count() {
            return ++thread_count;
        }

        uint32_t decrease_thread_count() {
            return --thread_count;
        }

        void logon(int *logon_request, bool rendezvous);
        bool logon_cancel(int *logon_request, bool rendezvous);

        void rendezvous(int rendezvous_reason);

        void finish_logons();

        process() = default;

        process(kernel_system *kern, memory_system *mem, uint32_t uid,
            const std::string &process_name, const std::u16string &exe_path,
            const std::u16string &cmd_args, loader::e32img_ptr &img,
            const process_priority pri = process_priority::foreground);

        process(kernel_system *kern, memory_system *mem, uint32_t uid,
            const std::string &process_name, const std::u16string &exe_path,
            const std::u16string &cmd_args, loader::romimg_ptr &img,
            const process_priority pri = process_priority::foreground);

        bool run();

        void set_arg_slot(uint8_t slot, uint32_t data, size_t data_size);
        std::optional<pass_arg> get_arg_slot(uint8_t slot);

        process_uid_type get_uid_type();
        kernel_obj_ptr get_object(uint32_t handle);

        void *get_ptr_on_addr_space(address addr);

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

        page_table &get_page_table() {
            return page_tab;
        }

        void set_flags(const uint32_t new_flags) {
            flags = new_flags;
        }

        uint32_t get_flags() const {
            return flags;
        }

        process_priority get_priority() const {
            return priority;
        }

        void set_priority(const process_priority new_pri);
    };
}
