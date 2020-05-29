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

#include <kernel/btrace.h>
#include <kernel/change_notifier.h>
#include <kernel/chunk.h>
#include <kernel/codeseg.h>
#include <kernel/kernel_obj.h>
#include <kernel/library.h>
#include <kernel/msgqueue.h>
#include <kernel/mutex.h>
#include <kernel/object_ix.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/sema.h>
#include <kernel/timer.h>

#include <kernel/property.h>
#include <kernel/server.h>
#include <kernel/session.h>

#include <common/hash.h>

#include <kernel/ipc.h>
#include <mem/ptr.h>

#include <atomic>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace eka2l1 {
#define SYNCHRONIZE_ACCESS const std::lock_guard<std::mutex> guard(kern_lock)

    class posix_server;
    class ntimer;
    class memory_system;
    class manager_system;
    class io_system;
    class system;

    namespace kernel {
        class thread;

        using uid = std::uint32_t;
    }

    namespace manager {
        struct config_state;
    }

    using thread_ptr = kernel::thread *;
    using process_ptr = kernel::process *;
    using chunk_ptr = kernel::chunk *;
    using mutex_ptr = kernel::mutex *;
    using sema_ptr = kernel::semaphore *;
    using timer_ptr = kernel::timer *;
    using kernel_obj_ptr = kernel::kernel_obj *;
    using change_notifier_ptr = kernel::change_notifier *;
    using property_ptr = service::property *;
    using library_ptr = kernel::library *;
    using codeseg_ptr = kernel::codeseg *;
    using property_ref_ptr = service::property_reference *;

    using kernel_obj_unq_ptr = std::unique_ptr<kernel::kernel_obj>;
    using prop_ident_pair = std::pair<int, int>;

    /*! \brief Check for template type and returns the right kernel::object_type value
    */
    template <typename T>
    constexpr kernel::object_type get_object_type() {
        if constexpr (std::is_same_v<T, kernel::process>) {
            return kernel::object_type::process;
        } else if constexpr (std::is_same_v<T, kernel::thread>) {
            return kernel::object_type::thread;
        } else if constexpr (std::is_same_v<T, kernel::chunk>) {
            return kernel::object_type::chunk;
        } else if constexpr (std::is_same_v<T, kernel::library>) {
            return kernel::object_type::library;
        } else if constexpr (std::is_same_v<T, kernel::mutex>) {
            return kernel::object_type::mutex;
        } else if constexpr (std::is_same_v<T, kernel::semaphore>) {
            return kernel::object_type::sema;
        } else if constexpr (std::is_same_v<T, kernel::timer>) {
            return kernel::object_type::timer;
        } else if constexpr (std::is_same_v<T, service::property>) {
            return kernel::object_type::prop;
        } else if constexpr (std::is_same_v<T, service::server>) {
            return kernel::object_type::server;
        } else if constexpr (std::is_same_v<T, service::session>) {
            return kernel::object_type::session;
        } else if constexpr (std::is_same_v<T, kernel::codeseg>) {
            return kernel::object_type::codeseg;
        } else if constexpr (std::is_same_v<T, kernel::change_notifier>) {
            return kernel::object_type::change_notifier;
        } else if constexpr (std::is_same_v<T, service::property_reference>) {
            return kernel::object_type::prop_ref;
        } else if constexpr (std::is_same_v<T, kernel::msg_queue>) {
            return kernel::object_type::msg_queue;
        } else {
            throw std::runtime_error("Unknown kernel object type. Make sure to add new type here");
            return kernel::object_type::unk;
        }
    }

    struct find_handle {
        int index;
        uint32_t object_id;
    };

    namespace arm {
        class arm_interface;
    }

    namespace common {
        class chunkyseri;
    }

    class kernel_system {
        friend class debugger_base;
        friend class imgui_debugger;
        friend class gdbstub;
        friend class kernel::process;

        /* Kernel objects map */
        std::array<ipc_msg_ptr, 0x1000> msgs;

        /* End kernel objects map */
        std::mutex kern_lock;
        std::shared_ptr<kernel::thread_scheduler> thr_sch;

        std::vector<kernel_obj_unq_ptr> threads;
        std::vector<kernel_obj_unq_ptr> processes;
        std::vector<kernel_obj_unq_ptr> servers;
        std::vector<kernel_obj_unq_ptr> sessions;
        std::vector<kernel_obj_unq_ptr> props;
        std::vector<kernel_obj_unq_ptr> prop_refs;
        std::vector<kernel_obj_unq_ptr> chunks;
        std::vector<kernel_obj_unq_ptr> mutexes;
        std::vector<kernel_obj_unq_ptr> semas;
        std::vector<kernel_obj_unq_ptr> change_notifiers;
        std::vector<kernel_obj_unq_ptr> libraries;
        std::vector<kernel_obj_unq_ptr> codesegs;
        std::vector<kernel_obj_unq_ptr> timers;
        std::vector<kernel_obj_unq_ptr> message_queues;

        std::unique_ptr<kernel::btrace> btrace_inst;

        ntimer *timing;
        manager_system *mngr;
        memory_system *mem;
        hle::lib_manager *libmngr;
        io_system *io;
        system *sys;

        void *rom_map;

        /* Contains the EPOC version */
        epocver kern_ver = epocver::epoc94;

        //! Handles for some globally shared processes
        kernel::object_ix kernel_handles;

        mutable std::atomic<uint32_t> uid_counter;

        std::uint64_t base_time;
        int realtime_ipc_signal_evt;

        manager::config_state *conf;

        void setup_new_process(process_ptr pr);

    public:
        uint32_t next_uid() const;

        explicit kernel_system()
            : uid_counter(0)
            , libmngr(nullptr)
            , io(nullptr)
            , sys(nullptr)
            , mem(nullptr)
            , mngr(nullptr)
            , timing(nullptr) {
        }

        ntimer *get_ntimer() {
            return timing;
        }

        memory_system *get_memory_system() {
            return mem;
        }

        hle::lib_manager *get_lib_manager() {
            return libmngr;
        }

        system *get_system() {
            return sys;
        }

        kernel::btrace *get_btrace() {
            return btrace_inst.get();
        }

        std::uint64_t home_time();

        void init(system *esys, ntimer *sys, manager_system *mngrsys,
            memory_system *mem_sys, io_system *io_sys, hle::lib_manager *lib_sys,
            manager::config_state *conf, arm::arm_interface *cpu);

        void shutdown();

        std::shared_ptr<kernel::thread_scheduler> get_thread_scheduler() {
            return thr_sch;
        }

        void reschedule() {
            lock();
            thr_sch->reschedule();
            unlock();
        }

        void unschedule_wakeup() {
            thr_sch->unschedule_wakeup();
        }

        epocver get_epoc_version() const {
            return kern_ver;
        }

        // For user-provided EPOC version
        void set_epoc_version(const epocver ver) {
            kern_ver = ver;
        }

        int get_ipc_realtime_signal_event() const {
            return realtime_ipc_signal_evt;
        }

        void prepare_reschedule();

        ipc_msg_ptr create_msg(kernel::owner_type owner);
        ipc_msg_ptr get_msg(int handle);

        void free_msg(ipc_msg_ptr msg);

        /*! \brief Completely destroy a message. */
        void destroy_msg(ipc_msg_ptr msg);

        /* Fast duplication, unsafe */
        kernel::handle mirror(kernel::thread *own_thread, kernel::handle handle, kernel::owner_type owner);
        kernel::handle mirror(kernel_obj_ptr obj, kernel::owner_type owner);

        kernel::handle open_handle(kernel_obj_ptr obj, kernel::owner_type owner);
        kernel::handle open_handle_with_thread(kernel::thread *thr, kernel_obj_ptr obj, kernel::owner_type owner);

        std::optional<find_handle> find_object(const std::string &name, int start, kernel::object_type type, const bool use_full_name = false);

        void add_custom_server(std::unique_ptr<service::server> &svr) {
            if (!svr.get()) {
                return;
            }

            servers.push_back(std::move(svr));
        }

        bool destroy(kernel_obj_ptr obj);
        int close(kernel::handle handle);

        kernel_obj_ptr get_kernel_obj_raw(kernel::handle handle);

        bool notify_prop(prop_ident_pair ident);
        bool subscribe_prop(prop_ident_pair ident, int *request_sts);
        bool unsubscribe_prop(prop_ident_pair ident);

        property_ptr get_prop(int category, int key); // Get property by category and key

        kernel::thread *crr_thread();
        kernel::process *crr_process();

        process_ptr spawn_new_process(const std::u16string &path,
            const std::u16string &cmd_arg = u"", const kernel::uid promised_uid3 = 0,
            const std::uint32_t stack_size = 0);

        bool should_terminate();
        void do_state(common::chunkyseri &seri);

        codeseg_ptr pull_codeseg_by_uids(const kernel::uid uid0, const kernel::uid uid1,
            const kernel::uid uid2);

        codeseg_ptr pull_codeseg_by_ep(const address ep);

        // Expose for scripting, indeed very dirty
        std::vector<kernel_obj_unq_ptr> &get_process_list() {
            return processes;
        }

        std::vector<kernel_obj_unq_ptr> &get_thread_list() {
            return threads;
        }

        std::vector<kernel_obj_unq_ptr> &get_codeseg_list() {
            return codesegs;
        }

        /*! \brief Get kernel object by handle
        */
        template <typename T>
        T *get(const kernel::handle handle) {
            T *result = reinterpret_cast<T *>(get_kernel_obj_raw(handle));
            if (result->get_object_type() != get_object_type<T>()) {
                return nullptr;
            }

            return result;
        }

        template <typename T>
        T *get_by_name_and_type(const std::string &name, const kernel::object_type obj_type) {
            switch (obj_type) {
#define OBJECT_SEARCH(obj_type, obj_map)                                               \
    case kernel::object_type::obj_type: {                                              \
        auto res = std::find_if(obj_map.begin(), obj_map.end(), [&](const auto &rhs) { \
            return name == rhs->name();                                                \
        });                                                                            \
        if (res == obj_map.end())                                                      \
            return nullptr;                                                            \
        return reinterpret_cast<T *>(res->get());                                      \
    }

                OBJECT_SEARCH(mutex, mutexes)
                OBJECT_SEARCH(sema, semas)
                OBJECT_SEARCH(chunk, chunks)
                OBJECT_SEARCH(thread, threads)
                OBJECT_SEARCH(process, processes)
                OBJECT_SEARCH(change_notifier, change_notifiers)
                OBJECT_SEARCH(library, libraries)
                OBJECT_SEARCH(codeseg, codesegs)
                OBJECT_SEARCH(server, servers)
                OBJECT_SEARCH(prop, props)
                OBJECT_SEARCH(prop_ref, prop_refs)
                OBJECT_SEARCH(session, sessions)
                OBJECT_SEARCH(timer, timers)
                OBJECT_SEARCH(msg_queue, message_queues)

#undef OBJECT_SEARCH

            default:
                break;
            }

            return nullptr;
        }

        /*! \brief Get kernel object by name
        */
        template <typename T>
        T *get_by_name(const std::string &name) {
            const kernel::object_type obj_type = get_object_type<T>();
            return get_by_name_and_type<T>(name, obj_type);
        }

        /*! \brief Get kernel object by ID
        */
        template <typename T>
        T *get_by_id(const kernel::uid uid) {
            const kernel::object_type obj_type = get_object_type<T>();

            switch (obj_type) {
                // It's gurantee that object are sorted by unique id, by just adding it
                // Deletion will not affect that fact.
#define OBJECT_SEARCH(obj_type, obj_map)                                                                             \
    case kernel::object_type::obj_type: {                                                                            \
        auto res = std::lower_bound(obj_map.begin(), obj_map.end(), nullptr, [&](const auto &lhs, const auto &rhs) { \
            return lhs->unique_id() < uid;                                                                           \
        });                                                                                                          \
        if (res != obj_map.end()) {                                                                                  \
            return reinterpret_cast<T *>(res->get());                                                                \
        }                                                                                                            \
        return nullptr;                                                                                              \
    }

                OBJECT_SEARCH(mutex, mutexes)
                OBJECT_SEARCH(sema, semas)
                OBJECT_SEARCH(chunk, chunks)
                OBJECT_SEARCH(thread, threads)
                OBJECT_SEARCH(process, processes)
                OBJECT_SEARCH(change_notifier, change_notifiers)
                OBJECT_SEARCH(library, libraries)
                OBJECT_SEARCH(codeseg, codesegs)
                OBJECT_SEARCH(server, servers)
                OBJECT_SEARCH(prop, props)
                OBJECT_SEARCH(prop_ref, prop_refs)
                OBJECT_SEARCH(session, sessions)
                OBJECT_SEARCH(timer, timers)
                OBJECT_SEARCH(msg_queue, message_queues)

#undef OBJECT_SEARCH

            default:
                break;
            }

            return nullptr;
        }

        /*! \brief Create and add to object array.
        */
        template <typename T, typename... args>
        T *create(args... creation_arg) {
            constexpr kernel::object_type obj_type = get_object_type<T>();
            std::unique_ptr<T> obj;

            if constexpr (obj_type == kernel::object_type::server) {
                obj = std::make_unique<T>(sys, creation_arg...);
            } else {
                obj = std::make_unique<T>(this, creation_arg...);
            }

            const kernel::uid obj_uid = obj->unique_id();

#define ADD_OBJECT_TO_CONTAINER(type, container, additional_setup) \
    case type:                                                     \
        additional_setup;                                          \
        container.push_back(std::move(obj));                       \
        return reinterpret_cast<T *>(container.back().get());

            switch (obj_type) {
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::thread, threads, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::process, processes, setup_new_process(reinterpret_cast<process_ptr>(obj.get())));
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::chunk, chunks, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::server, servers, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::prop, props, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::prop_ref, prop_refs, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::session, sessions, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::library, libraries, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::timer, timers, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::mutex, mutexes, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::sema, semas, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::change_notifier, change_notifiers, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::codeseg, codesegs, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::msg_queue, message_queues, )

            default:
                break;
            }

            return nullptr;
        }

        template <typename T, typename... args>
        std::pair<kernel::handle, T *> create_and_add(kernel::owner_type owner,
            args... creation_args) {
            T *obj = create<T>(creation_args...);
            return std::make_pair(open_handle(obj, owner), obj);
        }

        template <typename T, typename... args>
        std::pair<kernel::handle, T *> create_and_add_thread(kernel::owner_type owner,
            kernel::thread *thr, args... creation_args) {
            T *obj = create<T>(creation_args...);
            return std::make_pair(open_handle_with_thread(thr, obj, owner), obj);
        }

        // Lock the kernel
        void lock() {
            kern_lock.lock();
        }

        // Unlock the kernel
        void unlock() {
            kern_lock.unlock();
        }

        bool map_rom(const mem::vm_address addr, const std::string &path);
    };
}
