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

#include <arm/jit_factory.h>
#include <common/resource.h>

#include <kernel/chunk.h>
#include <kernel/wait_obj.h>

#include <ipc.h>
#include <ptr.h>

namespace eka2l1 {
    class kernel_system;
    class memory;

    namespace kernel {
        class mutex;
        class semaphore;
    }

    using chunk_ptr = std::shared_ptr<kernel::chunk>;
    using mutex_ptr = std::shared_ptr<kernel::mutex>;
    using sema_ptr = std::shared_ptr<kernel::semaphore>;

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
            priorty_absolute_forground = 400,
            priority_absolute_high = 500
        };

        struct tls_slot {
            int handle;
            uint32_t uid;
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

            std::vector<SBlock> blocks;

            // We don't use this. We use our own heap
            ptr<void> tls_heap;
            std::array<tls_slot, 50> tls_slots;
        };

        class thread : public wait_obj {
            friend class thread_scheduler;
            friend class mutex;

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

            chunk_ptr stack_chunk;
            chunk_ptr name_chunk;
            chunk_ptr tls_chunk;

            thread_local_data ldata;

            std::shared_ptr<thread_scheduler> scheduler; // The scheduler that schedules this thread
            std::vector<mutex_ptr> held_mutexes;
            std::vector<mutex_ptr> pending_mutexes;

            sema_ptr request_sema;
            ipc_msg_ptr sync_msg;

            void reset_thread_ctx(uint32_t entry_point, uint32_t stack_top);
            void create_stack_metadata(ptr<void> stack_ptr, uint32_t name_len, address name_ptr, address epa);

        public:
            thread();
            thread(kernel_system *kern, memory_system *mem, kernel::owner_type owner, kernel::uid owner_id, kernel::access_type access,
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

            bool run();
            bool stop();

            bool sleep(int64_t ns);
            bool resume();

            bool should_wait(const kernel::uid id) override;
            void acquire(const kernel::uid id) override;

            // Physically we can't compare thread.
            bool operator>(const thread &rhs);
            bool operator<(const thread &rhs);
            bool operator==(const thread &rhs);
            bool operator>=(const thread &rhs);
            bool operator<=(const thread &rhs);

            tls_slot *get_free_tls_slot(uint32_t handle, uint32_t dll_uid);
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
        };

        using thread_ptr = std::shared_ptr<kernel::thread>;
    }
}
