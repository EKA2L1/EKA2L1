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

#include <core/arm/jit_interface.h>
#include <core/kernel/chunk.h>
#include <core/kernel/kernel_obj.h>
#include <core/kernel/mutex.h>
#include <core/kernel/object_ix.h>

#include <core/kernel/process.h>
#include <core/kernel/scheduler.h>
#include <core/kernel/sema.h>
#include <core/kernel/timer.h>

#include <core/services/property.h>
#include <core/services/server.h>
#include <core/services/session.h>

#include <common/hash.h>

#include <core/ipc.h>
#include <core/ptr.h>

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
    using process_ptr = std::shared_ptr<kernel::process>;
    using chunk_ptr = std::shared_ptr<kernel::chunk>;
    using mutex_ptr = std::shared_ptr<kernel::mutex>;
    using sema_ptr = std::shared_ptr<kernel::semaphore>;
    using timer_ptr = std::shared_ptr<kernel::timer>;
    using kernel_obj_ptr = std::shared_ptr<kernel::kernel_obj>;

    using property_ptr = std::shared_ptr<service::property>;

    using prop_ident_pair = std::pair<int, int>;

    struct find_handle {
        int index;
        uint64_t object_id;
    };

    class kernel_system {
        friend class process;

        /* Kernel objects map */
        std::unordered_map<prop_ident_pair, int *> prop_request_queue;
        std::array<ipc_msg_ptr, 0x80> msgs;

        /* End kernel objects map */
        std::mutex mut;
        std::shared_ptr<kernel::thread_scheduler> thr_sch;

        std::vector<kernel_obj_ptr> objects;

        timing_system *timing;
        manager_system *mngr;
        memory_system *mem;
        hle::lib_manager *libmngr;
        io_system *io;
        system *sys;

        /* Contains the EPOC version */
        epocver kern_ver = epocver::epoc9;

        //! Handles for some globally shared processes
        kernel::object_ix kernel_handles;

        uint32_t create_handle_lastest(kernel::owner_type owner);

        mutable std::atomic<uint64_t> uid_counter;

    public:
        uint64_t next_uid() const;

        memory_system *get_memory_system() {
            return mem;
        }

        void init(system *esys, timing_system *sys, manager_system *mngrsys,
            memory_system *mem_sys, io_system *io_sys, hle::lib_manager *lib_sys, arm::jit_interface *cpu);

        void shutdown();

        std::shared_ptr<kernel::thread_scheduler> get_thread_scheduler() {
            return thr_sch;
        }

        process_ptr get_process(std::string &name);
        process_ptr get_process(uint32_t handle);

        void reschedule() {
            thr_sch->reschedule();
        }

        void unschedule_wakeup() {
            thr_sch->unschedule_wakeup();
        }

        void processing_requests();

        epocver get_epoc_version() const {
            return kern_ver;
        }

        // For user-provided EPOC version
        void set_epoc_version(const epocver ver) {
            kern_ver = ver;
        }

        void prepare_reschedule();

        // Create a chunk with these condition
        uint32_t create_chunk(std::string name, const address bottom, const address top, const size_t size, prot protection,
            kernel::chunk_type type, kernel::chunk_access access, kernel::chunk_attrib attrib,
            kernel::owner_type owner);

        uint32_t create_thread(kernel::owner_type owner, process_ptr own_pr, kernel::access_type access,
            const std::string &name, const address epa, const size_t stack_size,
            const size_t min_heap_size, const size_t max_heap_size,
            ptr<void> usrdata = 0,
            kernel::thread_priority pri = kernel::priority_normal);

        uint32_t create_mutex(std::string name, bool init_locked,
            kernel::owner_type own,
            kernel::access_type access = kernel::access_type::local_access);

        uint32_t create_sema(std::string sema_name,
            int32_t init_count,
            int32_t max_count,
            kernel::owner_type own_type,
            kernel::access_type access = kernel::access_type::local_access);

        uint32_t create_timer(std::string name, kernel::reset_type rt,
            kernel::owner_type owner,
            kernel::access_type access = kernel::access_type::local_access);

        uint32_t create_server(std::string name);
        uint32_t create_session(server_ptr cnn_svr, int async_slots);

        ipc_msg_ptr create_msg(kernel::owner_type owner);
        void free_msg(ipc_msg_ptr msg);

        /*! \brief Completely destroy a message. */
        void destroy_msg(ipc_msg_ptr msg);

        uint32_t create_prop(service::property_type pt, uint32_t pre_allocated);

        /* Fast duplication, unsafe */
        uint32_t mirror(thread_ptr own_thread, uint32_t handle, kernel::owner_type owner);

        uint32_t open_handle(kernel_obj_ptr obj, kernel::owner_type owner);

        std::optional<find_handle> find_object(const std::string &name, int start, kernel::object_type type);

        void add_custom_server(server_ptr svr) {
            objects.push_back(std::move(std::dynamic_pointer_cast<kernel::kernel_obj>(svr)));
        }

        bool destroy(kernel_obj_ptr obj);
        bool close(uint32_t handle);

        kernel_obj_ptr get_kernel_obj(uint32_t handle);
        kernel_obj_ptr get_kernel_obj_by_id(uint64_t id);

        thread_ptr get_thread_by_name(const std::string &name);
        thread_ptr kernel_system::get_thread_by_handle(uint32_t handle);

        session_ptr get_session(uint32_t handle);

        server_ptr get_server(uint32_t handle);
        server_ptr get_server_by_name(const std::string name);

        std::vector<thread_ptr> get_all_thread_own_process(process_ptr pr);

        bool run_thread(uint32_t handle);
        bool run_process(uint32_t handle);

        uint32_t spawn_new_process(std::string &path, std::string name, uint32_t uid, kernel::owner_type owner = kernel::owner_type::kernel);
        uint32_t spawn_new_process(uint32_t uid, kernel::owner_type owner = kernel::owner_type::kernel);

        bool destroy_process(process_ptr pr);
        bool destroy_process(const kernel::uid id);
        bool destroy_all_processes();

        bool notify_prop(prop_ident_pair ident);
        bool subscribe_prop(prop_ident_pair ident, int *request_sts);
        bool unsubscribe_prop(prop_ident_pair ident);

        property_ptr get_prop(int cagetory, int key); // Get property by category and key
        property_ptr get_prop(uint32_t handle);

        thread_ptr crr_thread();
        process_ptr crr_process();

        void set_handle_owner_type(int handle);

        bool should_terminate();
    };
}
