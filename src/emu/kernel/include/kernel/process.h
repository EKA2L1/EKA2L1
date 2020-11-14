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

#include <common/linked.h>
#include <common/container.h>

#include <kernel/common.h>
#include <kernel/kernel_obj.h>
#include <kernel/mutex.h>
#include <kernel/object_ix.h>
#include <kernel/thread.h>
#include <mem/process.h>
#include <utils/reqsts.h>
#include <utils/sec.h>

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace eka2l1 {
    class kernel_system;

    namespace kernel {
        class chunk;
    }

    using chunk_ptr = kernel::chunk *;
    using mutex_ptr = kernel::mutex *;

    namespace common {
        class chunkyseri;
    }
}

namespace eka2l1::kernel {
    class thread_scheduler;

    struct process_info {
        ptr<void> code_where;
        std::uint64_t size;
    };

    struct pass_arg {
        bool used = false;
        std::vector<std::uint8_t> data;

        void do_state(common::chunkyseri &seri);
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

    struct memory_info {
        address rt_code_addr;
        address rt_code_size;

        address rt_const_data_addr;
        address rt_const_data_size;

        address rt_initialized_data_addr;
        address rt_initialized_data_size;

        address rt_bss_addr;
        address rt_bss_size;
    };

    class codeseg;
    using codeseg_ptr = kernel::codeseg *;

    using process_uid_type_change_callback = std::function<void(void*, const process_uid_type &)>;
    using process_uid_type_change_callback_elem = std::pair<void*, process_uid_type_change_callback>;

    class process : public kernel_obj {
        friend class eka2l1::kernel_system;
        friend class thread_scheduler;

        enum {
            FLAG_NONE = 0,
            FLAG_KERNEL_PROCESS = 1 << 0,
            FLAG_CRITICAL = 1 << 1
        };

        mem::mem_model_process_impl mm_impl_;

        process_uid_type uids;
        thread_ptr primary_thread;

        std::string process_name;

        codeseg_ptr codeseg;

        kernel_system *kern;
        memory_system *mem;

        std::array<pass_arg, 16> args;

        std::u16string exe_path;
        std::u16string cmd_args;

        object_ix process_handles;
        std::uint32_t flags;
        process_priority priority;
        int exit_reason = 0;
        entity_exit_type exit_type;

        std::vector<epoc::notify_info> logon_requests;
        std::vector<epoc::notify_info> rendezvous_requests;

        std::uint32_t thread_count = 0;

        mutex_ptr dll_lock;
        epoc::security_info sec_info;

        common::roundabout thread_list;

        std::vector<kernel::process*> child_processes_;
        kernel::process *parent_process_;

        std::uint32_t time_delay_;
        bool setting_inheritence_;

        std::int32_t generation_;

        // Это оскорбления, первое слово оскорбляет человека, а второе говорят для
        // увеличения эмоций.
        common::identity_container<process_uid_type_change_callback_elem> uid_change_callbacks;

    protected:
        std::int32_t refresh_generation();

        void reload_compat_setting();
        void create_prim_thread(uint32_t code_addr, uint32_t ep_off, uint32_t stack_size, uint32_t heap_min,
            uint32_t heap_max, kernel::thread_priority pri);

        void detatch_from_parent();

    public:
        uint32_t increase_thread_count() {
            return ++thread_count;
        }

        uint32_t decrease_thread_count() {
            return --thread_count;
        }

        mem::mem_model_process *get_mem_model() {
            return mm_impl_.get();
        }

        thread_ptr get_primary_thread() {
            return primary_thread;
        }

        void logon(eka2l1::ptr<epoc::request_status> logon_request, bool rendezvous);
        bool logon_cancel(eka2l1::ptr<epoc::request_status> logon_request, bool rendezvous);

        void rendezvous(int rendezvous_reason);

        void finish_logons();

        void construct_with_codeseg(codeseg_ptr codeseg, uint32_t stack_size, uint32_t heap_min, uint32_t heap_max,
            const process_priority pri);

        explicit process(kernel_system *kern, memory_system *mem, const std::string &process_name,
            const std::u16string &exe_path, const std::u16string &cmd_args);

        ~process() = default;

        void destroy() override;
        bool run();

        std::string name() const override;
        void rename(const std::string &new_name) override;

        void set_arg_slot(uint8_t slot, std::uint8_t *data, size_t data_size);
        std::optional<pass_arg> get_arg_slot(uint8_t slot);

        void mark_slot_free(std::uint8_t slot) {
            assert(slot < 16);
            args[slot].used = false;
        }

        process_uid_type get_uid_type();
        void set_uid_type(const process_uid_type &type);

        /**
         * @brief Register a callback when the UID type of this process is changed.
         * 
         * @param userdata          The data to passed to the callback when it's called.
         * @param callback          The callback to register.
         * 
         * @returns The handle to the callback.
         */
        std::size_t register_uid_type_change_callback(void *userdata, process_uid_type_change_callback callback);
        
        bool unregister_uid_type_change_callback(const std::size_t handle);

        kernel_obj_ptr get_object(const std::uint32_t handle);

        void *get_ptr_on_addr_space(address addr);

        void get_memory_info(memory_info &info);

        std::u16string get_cmd_args() const {
            return cmd_args;
        }

        std::u16string get_exe_path() const {
            return exe_path;
        }

        codeseg_ptr get_codeseg() {
            return codeseg;
        }

        std::uint32_t get_uid() {
            return std::get<2>(uids);
        }

        void set_is_kernel_process(const bool value) {
            flags &= ~FLAG_KERNEL_PROCESS;

            if (value) {
                flags |= FLAG_KERNEL_PROCESS;
            }
        }

        bool is_kernel_process() const {
            return flags & FLAG_KERNEL_PROCESS;
        }

        void set_flags(const std::uint32_t new_flags) {
            flags = new_flags;
        }

        std::uint32_t get_flags() const {
            return flags;
        }

        std::uint32_t get_entry_point_address();

        process_priority get_priority() const {
            return priority;
        }

        epoc::security_info get_sec_info();

        void set_priority(const process_priority new_pri);

        void wait_dll_lock();
        void signal_dll_lock(kernel::thread *callee);

        int get_exit_reason() const {
            return exit_reason;
        }

        std::size_t get_total_open_handles() const {
            return process_handles.total_open();
        }

        /**
         * \brief Check if the process's security satisfy the given security policy.
         * 
         * \param policy  Security policy to test against.
         * \param missing Optional variable, used to fill missing info the process needed
         *                in order to pass the policy.
         * 
         * \returns       True if pass.
         */
        bool satisfy(epoc::security_policy &policy, epoc::security_info *missing = nullptr);

        /**
         * \brief Check if the process has the following capabilities given in the set.
         */
        bool has(epoc::capability_set &cap_set);

        /**
         * @brief Attach another process as a child.
         * 
         * A child process under the control may inherits settings from its parent. This is a mode that's specifically
         * exists for emulator usage (when game spawns another process doing actual work for example).
         * 
         * The most usage you can see in this exists in the LaunchApp implementation in App List server.
         * 
         * @param   pr  The process to be attached as child.
         */
        void add_child_process(kernel::process *pr);

        entity_exit_type get_exit_type() const {
            return exit_type;
        }

        void set_exit_type(const entity_exit_type t) {
            exit_type = t;
        }

        std::uint32_t get_time_delay() const;
        void set_time_delay(const std::uint32_t delay);
        
        /**
         * @brief       Get the process where we should inherit settings from.
         * 
         * This may stack up for a big while.
         * 
         * @returns     Process to get settings from. Return itself if no inheritence is specified.
         */
        kernel::process *get_final_setting_process();

        bool get_child_inherit_setting() const {
            return setting_inheritence_;
        }

        void set_child_inherit_setting(const bool enable) {
            setting_inheritence_ = enable;
        }

        kernel::process *get_parent_process() {
            return parent_process_;
        }

        void do_state(common::chunkyseri &seri);

        common::roundabout &get_thread_list() {
            return thread_list;
        }
    };
}
