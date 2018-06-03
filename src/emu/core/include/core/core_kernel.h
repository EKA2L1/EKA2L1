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

#include <arm/jit_interface.h>
#include <kernel/kernel_obj.h>
#include <kernel/scheduler.h>
#include <kernel/chunk.h>
#include <process.h>
#include <ptr.h>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>

namespace eka2l1 {
    class timing_system;
    class memory_system;
    class manager_system;
    class io_system;

    namespace kernel {
        class thread;

        using uid = uint64_t;
    }

    using thread_ptr = std::shared_ptr<kernel::thread>;
    using process_ptr = std::shared_ptr<process>;
    using chunk_ptr = std::shared_ptr<kernel::chunk>;
    using kernel_obj_ptr = std::shared_ptr<kernel::kernel_obj>;

    class kernel_system {
        friend class process;

        std::atomic<kernel::uid> crr_uid;
        kernel::uid crr_process_id;

        std::map<kernel::uid, thread_ptr> threads;
        std::map<kernel::uid, chunk_ptr> chunks;

        std::mutex mut;
        std::shared_ptr<kernel::thread_scheduler> thr_sch;

        std::map<uint32_t, process_ptr> processes;

        timing_system *timing;
        manager_system *mngr;
        memory_system *mem;
        hle::lib_manager *libmngr;
        io_system *io;

    public:
        void init(timing_system *sys, manager_system *mngrsys,
            memory_system *mem_sys, io_system *io_sys, hle::lib_manager *lib_sys, arm::jit_interface *cpu);

        void shutdown();

        std::shared_ptr<kernel::thread_scheduler> get_thread_scheduler() {
            return thr_sch;
        }

        void reschedule() {
            thr_sch->reschedule();
        }

        void unschedule_wakeup() {
            thr_sch->unschedule_wakeup();
        }

        void unschedule(kernel::uid thread_id) {
            thr_sch->unschedule(thread_id);
        }

        kernel::uid next_uid();
        kernel::uid get_id_base_owner(kernel::owner_type owner) const;

        // Create a chunk with these condition
        chunk_ptr create_chunk(std::string name, const address bottom, const address top, const size_t size, prot protection,
            kernel::chunk_type type, kernel::chunk_access access, kernel::chunk_attrib attrib,
            kernel::owner_type owner);

        bool close_chunk(kernel::uid id);
        bool close_thread(kernel::uid id);

        bool close(kernel::uid id);

        void set_closeable(kernel::uid id, bool opt);
        bool get_closeable(kernel::uid id);

        kernel_obj_ptr get_kernel_obj(kernel::uid id);

        thread_ptr add_thread(uint32_t owner, const std::string &name, const address epa, const size_t stack_size,
            const size_t min_heap_size, const size_t max_heap_size,
            void *usrdata = nullptr,
            kernel::thread_priority pri = kernel::priority_normal);

        bool run_thread(kernel::uid thr);

        process *spawn_new_process(std::string &path, std::string name, uint32_t uid);
        process *spawn_new_process(uint32_t uid);

        bool close_process(process *pr);
        bool close_all_processes();

        thread_ptr crr_thread();
    };
}

