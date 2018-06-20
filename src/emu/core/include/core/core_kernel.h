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
#include <kernel/chunk.h>
#include <kernel/kernel_obj.h>
#include <kernel/mutex.h>
#include <kernel/scheduler.h>
#include <kernel/sema.h>
#include <kernel/timer.h>

#include <ipc.h>
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
    class system;

    namespace kernel {
        class thread;

        using uid = uint64_t;
    }

    using thread_ptr = std::shared_ptr<kernel::thread>;
    using process_ptr = std::shared_ptr<process>;
    using chunk_ptr = std::shared_ptr<kernel::chunk>;
    using mutex_ptr = std::shared_ptr<kernel::mutex>;
    using sema_ptr = std::shared_ptr<kernel::semaphore>;
    using timer_ptr = std::shared_ptr<kernel::timer>;
    using kernel_obj_ptr = std::shared_ptr<kernel::kernel_obj>;

    class kernel_system {
        friend class process;

        std::atomic<kernel::uid> crr_uid;

        // Message is not based on kernel object, it should have its own id counter.
        std::atomic<kernel::uid> msg_crr_uid;

        kernel::uid crr_process_id;

        /* Kernel objects map */

        std::map<kernel::uid, thread_ptr> threads;
        std::map<kernel::uid, chunk_ptr> chunks;
        std::map<kernel::uid, mutex_ptr> mutexes;
        std::map<kernel::uid, sema_ptr> semas;
        std::map<kernel::uid, timer_ptr> timers;

        std::unordered_map<kernel::uid, ipc_msg_ptr> msgs;

        /* End kernel objects map */

        std::mutex mut;
        std::shared_ptr<kernel::thread_scheduler> thr_sch;

        std::map<uint32_t, process_ptr> processes;

        timing_system *timing;
        manager_system *mngr;
        memory_system *mem;
        hle::lib_manager *libmngr;
        io_system *io;
        system *sys;

        /* Contains the EPOC version */
        epocver kern_ver = epocver::epoc9;

    public:
        void init(system *esys, timing_system *sys, manager_system *mngrsys,
            memory_system *mem_sys, io_system *io_sys, hle::lib_manager *lib_sys, arm::jit_interface *cpu);

        void shutdown();

        std::shared_ptr<kernel::thread_scheduler> get_thread_scheduler() {
            return thr_sch;
        }

        process_ptr get_process(uint32_t uid) {
            return processes[uid];
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

        epocver get_epoc_version() const {
            return kern_ver;
        }

        // For user-provided EPOC version
        void set_epoc_version(const epocver ver) {
            kern_ver = ver;
        }

        kernel::uid next_uid();
        kernel::uid get_id_base_owner(kernel::owner_type owner) const;

        // Create a chunk with these condition
        chunk_ptr create_chunk(std::string name, const address bottom, const address top, const size_t size, prot protection,
            kernel::chunk_type type, kernel::chunk_access access, kernel::chunk_attrib attrib,
            kernel::owner_type owner,
            int64_t owner_id = -1);

        thread_ptr add_thread(kernel::owner_type owner, kernel::uid owner_id, kernel::access_type access,
            const std::string &name, const address epa, const size_t stack_size,
            const size_t min_heap_size, const size_t max_heap_size,
            ptr<void> usrdata = nullptr,
            kernel::thread_priority pri = kernel::priority_normal);

        mutex_ptr create_mutex(std::string name, bool init_locked,
            kernel::owner_type own,
            kernel::uid own_id = -1,
            kernel::access_type access = kernel::access_type::local_access);

        sema_ptr create_sema(std::string sema_name,
            int32_t init_count,
            int32_t max_count,
            kernel::owner_type own_type,
            kernel::uid own_id = -1,
            kernel::access_type access = kernel::access_type::local_access);

        timer_ptr create_timer(std::string name, kernel::reset_type rt,
            kernel::owner_type owner,
            kernel::uid own_id = -1,
            kernel::access_type access = kernel::access_type::local_access);

        /*! \brief Create an IPC message. 
         *
         * First, look up to see if there is any messsage free to use. Mark message found as not free and return it.
         * If there isn't any free message, create a new one and store it in an unordered map.
        */
        ipc_msg_ptr create_msg(kernel::owner_type owner);

        /*! \brief Free a message. */
        void free_msg(ipc_msg_ptr msg);

        /*! \brief Completely destroy a message. */
        void destroy_msg(ipc_msg_ptr msg);

        bool close_chunk(kernel::uid id);
        bool close_thread(kernel::uid id);
        bool close_timer(kernel::uid id);
        bool close_sema(kernel::uid id);
        bool close_mutex(kernel::uid id);

        bool close(kernel::uid id);

        void set_closeable(kernel::uid id, bool opt);
        bool get_closeable(kernel::uid id);

        kernel_obj_ptr get_kernel_obj(kernel::uid id);
        thread_ptr get_thread_by_name(const std::string &name);

        bool run_thread(kernel::uid thr);

        process *spawn_new_process(std::string &path, std::string name, uint32_t uid);
        process *spawn_new_process(uint32_t uid);

        bool close_process(process *pr);
        bool close_process(const kernel::uid id);
        bool close_all_processes();

        kernel::uid crr_process() const {
            return crr_process_id;
        }

        thread_ptr crr_thread();
    };
}
