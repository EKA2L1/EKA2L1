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

#include <array>
#include <condition_variable>
#include <memory>
#include <optional>
#include <string>

#include <common/resource.h>
#include <core/arm/jit_factory.h>

#include <core/kernel/object_ix.h>
#include <core/kernel/chunk.h>
#include <core/kernel/wait_obj.h>

#include <core/ipc.h>
#include <core/ptr.h>

namespace eka2l1 {
    class kernel_system;
    class memory;

    namespace kernel {
        class mutex;
        class semaphore;
        class process;
    }

    using chunk_ptr = std::shared_ptr<kernel::chunk>;
    using mutex_ptr = std::shared_ptr<kernel::mutex>;
    using sema_ptr = std::shared_ptr<kernel::semaphore>;
    using process_ptr = std::shared_ptr<kernel::process>;

    namespace kernel {
        using address = uint32_t;
        using thread_stack = common::resource<address>;
        using thread_stack_ptr = std::unique_ptr<thread_stack>;

        class thread_scheduler;

        enum class thread_state {
            run,
            wait,
            ready,
            stop,
            wait_fast_sema, // Wait for semaphore
            wait_dfc, // Unused
            wait_hle // Wait in case an HLE event is taken place - e.g GUI
        };

        enum thread_priority {
            priority_null = -30,
            priority_much_less = -20,
            priority_less = -10,
            priority_normal = 0,
            priority_more = 10,
            priority_much_more = 20,
            priority_real_time = 30,
            priority_absolute_very_low = 100,
            priority_absolute_low = 200,
            priority_absolute_background = 300,
            priorty_absolute_foreground = 400,
            priority_absolute_high = 500
        };

        struct tls_slot {
            int handle = -1;
            int uid = -1;
            ptr<void> ptr;
        };

        struct SBlock {
            int offset;
            int size;
            eka2l1::ptr<void> block_ptr;

            bool free = true;
        };

        struct thread_local_data {
            ptr<void> heap;
            ptr<void> scheduler;
            ptr<void> trap_handler;
            uint32_t thread_id;

            // We don't use this. We use our own heap
            ptr<void> tls_heap;
            std::array<tls_slot, 50> tls_slots;
        };

        using wait_obj_ptr = std::shared_ptr<wait_obj>;

        class thread : public wait_obj {
            friend class thread_scheduler;
            friend class kernel_system;
            friend class mutex;

            process_ptr own_process;

            thread_state state;
            std::mutex mut;
            std::condition_variable todo;

            // Thread context to save when suspend the execution
            arm::jit_interface::thread_context ctx;

            int priority;

            size_t stack_size;
            size_t min_heap_size, max_heap_size;

            ptr<void> usrdata;

            memory_system *mem;
            timing_system *timing;

            uint32_t lrt;

            uint32_t stack_chunk;
            uint32_t name_chunk;
            uint32_t tls_chunk;

            thread_local_data ldata;

            std::shared_ptr<thread_scheduler> scheduler; // The scheduler that schedules this thread
            std::vector<kernel::mutex*> held_mutexes;
            std::vector<kernel::mutex*> pending_mutexes;

            uint32_t request_sema;
            ipc_msg_ptr sync_msg;

            void reset_thread_ctx(uint32_t entry_point, uint32_t stack_top);
            void create_stack_metadata(ptr<void> stack_ptr, uint32_t name_len, address name_ptr, address epa);

            int leave_depth = -1;

            object_ix thread_handles;

        public:
            kernel_obj_ptr get_object(uint32_t handle);

            std::vector<wait_obj *> waits_on;

            void increase_leave_depth() {
                leave_depth++;
            }

            void decrease_leave_depth() {
                leave_depth--;
            }

            bool is_invalid_leave() const {
                return leave_depth > 0;
            }

            thread();
            thread(kernel_system *kern, memory_system *mem, process_ptr owner, kernel::access_type access,
                const std::string &name, const address epa, const size_t stack_size,
                const size_t min_heap_size, const size_t max_heap_size,
                ptr<void> usrdata = nullptr,
                thread_priority pri = priority_normal);

            thread_state current_state() const {
                return state;
            }

            int current_priority() const {
                return priority;
            }

            void current_state(thread_state st) {
                state = st;
            }

            ipc_msg_ptr &get_sync_msg() {
                return sync_msg;
            }

            bool should_wait(thread_ptr thr) override;
            void acquire(thread_ptr thr) override;

            // Physically we can't compare thread.
            bool operator>(const thread &rhs);
            bool operator<(const thread &rhs);
            bool operator==(const thread &rhs);
            bool operator>=(const thread &rhs);
            bool operator<=(const thread &rhs);

            tls_slot *get_tls_slot(uint32_t handle, uint32_t dll_uid);
            void close_tls_slot(tls_slot &slot);

            thread_local_data &get_local_data() {
                return ldata;
            }

            std::shared_ptr<thread_scheduler> get_scheduler() {
                return scheduler;
            }

            void update_priority();

            void wait_for_any_request();
            void signal_request();

            process_ptr owning_process() {
                return own_process;
            }

            void owning_process(process_ptr pr) {
                own_process = pr;
            }
        };

        using thread_ptr = std::shared_ptr<kernel::thread>;
    }
}
