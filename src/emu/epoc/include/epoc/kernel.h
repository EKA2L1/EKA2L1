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

#include <epoc/kernel/change_notifier.h>
#include <epoc/kernel/chunk.h>
#include <epoc/kernel/kernel_obj.h>
#include <epoc/kernel/library.h>
#include <epoc/kernel/mutex.h>
#include <epoc/kernel/object_ix.h>
#include <epoc/kernel/codeseg.h>
#include <epoc/kernel/process.h>
#include <epoc/kernel/scheduler.h>
#include <epoc/kernel/sema.h>
#include <epoc/kernel/timer.h>

#include <epoc/services/property.h>
#include <epoc/services/server.h>
#include <epoc/services/session.h>

#include <common/hash.h>

#include <epoc/ipc.h>
#include <epoc/ptr.h>

#include <atomic>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace eka2l1 {
    #define SYNCHRONIZE_ACCESS const std::lock_guard<std::mutex> guard(kern_lock)

    class posix_server;
    class timing_system;
    class memory_system;
    class manager_system;
    class io_system;
    class system;

    namespace kernel {
        class thread;

        using uid = std::uint32_t;
    }

    using thread_ptr = std::shared_ptr<kernel::thread>;
    using process_ptr = std::shared_ptr<kernel::process>;
    using chunk_ptr = std::shared_ptr<kernel::chunk>;
    using mutex_ptr = std::shared_ptr<kernel::mutex>;
    using sema_ptr = std::shared_ptr<kernel::semaphore>;
    using timer_ptr = std::shared_ptr<kernel::timer>;
    using kernel_obj_ptr = std::shared_ptr<kernel::kernel_obj>;
    using change_notifier_ptr = std::shared_ptr<kernel::change_notifier>;
    using property_ptr = std::shared_ptr<service::property>;
    using library_ptr = std::shared_ptr<kernel::library>;
    using codeseg_ptr = std::shared_ptr<kernel::codeseg>;

    using prop_ident_pair = std::pair<int, int>;
    
    /*! \brief Check for template type and returns the right kernel::object_type value
    */
    template <typename T>
    constexpr kernel::object_type get_object_type() {
        if constexpr(std::is_same_v<T, kernel::process>) {
            return kernel::object_type::process;
        } else if constexpr(std::is_same_v<T, kernel::thread>) {
            return kernel::object_type::thread;
        } else if constexpr(std::is_same_v<T, kernel::chunk>) {
            return kernel::object_type::chunk;
        } else if constexpr(std::is_same_v<T, kernel::library>) {
            return kernel::object_type::library;
        } else if constexpr(std::is_same_v<T, kernel::mutex>) {
            return kernel::object_type::mutex;
        } else if constexpr(std::is_same_v<T, kernel::semaphore>) {
            return kernel::object_type::sema;
        } else if constexpr(std::is_same_v<T, kernel::timer>) {
            return kernel::object_type::timer;
        } else if constexpr(std::is_same_v<T, service::property>) {
            return kernel::object_type::prop;
        } else if constexpr(std::is_same_v<T, service::server>) {
            return kernel::object_type::server;
        } else if constexpr(std::is_same_v<T, service::session>) {
            return kernel::object_type::session;
        } else if constexpr(std::is_same_v<T, kernel::codeseg>) {
            return kernel::object_type::codeseg;
        } else if constexpr(std::is_same_v<T, kernel::change_notifier>) {
            return kernel::object_type::change_notifier;
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

        friend class kernel::process;

        /* Kernel objects map */
        std::array<ipc_msg_ptr, 0x1000> msgs;

        /* End kernel objects map */
        std::mutex kern_lock;
        std::shared_ptr<kernel::thread_scheduler> thr_sch;
    
        std::vector<thread_ptr> threads;
        std::vector<process_ptr> processes;
        std::vector<server_ptr> servers;
        std::vector<session_ptr> sessions;
        std::vector<property_ptr> props;
        std::vector<chunk_ptr> chunks;
        std::vector<mutex_ptr> mutexes;
        std::vector<sema_ptr> semas;
        std::vector<change_notifier_ptr> change_notifiers;
        std::vector<library_ptr> libraries;
        std::vector<codeseg_ptr> codesegs;
        std::vector<timer_ptr> timers;

        timing_system *timing;
        manager_system *mngr;
        memory_system *mem;
        hle::lib_manager *libmngr;
        io_system *io;
        system *sys;

        /* Contains the EPOC version */
        epocver kern_ver = epocver::epoc94;

        //! Handles for some globally shared processes
        kernel::object_ix kernel_handles;

        mutable std::atomic<uint32_t> uid_counter;

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

        timing_system *get_timing_system() {
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

        void init(system *esys, timing_system *sys, manager_system *mngrsys,
            memory_system *mem_sys, io_system *io_sys, hle::lib_manager *lib_sys, arm::arm_interface *cpu);

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

        epocver get_epoc_version() const {
            return kern_ver;
        }

        // For user-provided EPOC version
        void set_epoc_version(const epocver ver) {
            kern_ver = ver;
        }

        void prepare_reschedule();

        ipc_msg_ptr create_msg(kernel::owner_type owner);
        ipc_msg_ptr get_msg(int handle);

        void free_msg(ipc_msg_ptr msg);

        /*! \brief Completely destroy a message. */
        void destroy_msg(ipc_msg_ptr msg);

        /* Fast duplication, unsafe */
        kernel::handle mirror(thread_ptr own_thread, kernel::handle handle, kernel::owner_type owner);
        kernel::handle mirror(kernel_obj_ptr obj, kernel::owner_type owner);

        kernel::handle open_handle(kernel_obj_ptr obj, kernel::owner_type owner);
        kernel::handle open_handle_with_thread(thread_ptr thr, kernel_obj_ptr obj, kernel::owner_type owner);

        std::optional<find_handle> find_object(const std::string &name, int start, kernel::object_type type);

        void add_custom_server(server_ptr svr) {
            SYNCHRONIZE_ACCESS;
            servers.push_back(std::move(svr));
        }

        bool destroy(kernel_obj_ptr obj);
        int close(kernel::handle handle);

        kernel_obj_ptr get_kernel_obj_raw(kernel::handle handle);

        bool notify_prop(prop_ident_pair ident);
        bool subscribe_prop(prop_ident_pair ident, int *request_sts);
        bool unsubscribe_prop(prop_ident_pair ident);

        property_ptr get_prop(int cagetory, int key); // Get property by category and key

        thread_ptr crr_thread();
        process_ptr crr_process();

        process_ptr spawn_new_process(const kernel::uid uid);
        process_ptr spawn_new_process(const std::u16string &path,
            const std::u16string &cmd_arg = u"", const kernel::uid promised_uid3 = 0, 
            const std::uint32_t stack_size = 0);

        bool should_terminate();
        void do_state(common::chunkyseri &seri);
        
        codeseg_ptr pull_codeseg_by_uids(const kernel::uid uid0, const kernel::uid uid1,
            const kernel::uid uid2);
            
        codeseg_ptr pull_codeseg_by_ep(const address ep);

        // Expose for scripting, indeed very dirty
        std::vector<process_ptr> &get_process_list() {
            return processes;
        }

        std::vector<thread_ptr> &get_thread_list() {
            return threads;
        }

        std::vector<codeseg_ptr> &get_codeseg_list() {
            return codesegs;
        }

        /*! \brief Get kernel object by handle
        */
        template <typename T>
        std::shared_ptr<T> get(const kernel::handle handle) {
            return std::reinterpret_pointer_cast<T>(get_kernel_obj_raw(handle));
        }

        template <typename T>
        constexpr std::shared_ptr<T> get_by_name_and_type(const std::string &name, const kernel::object_type
            obj_type) {
            switch (obj_type) {
            #define OBJECT_SEARCH(obj_type, obj_map)                                                   \
                case kernel::object_type::obj_type: {                                                  \
                    auto res = std::find_if(obj_map.begin(), obj_map.end(), [&](const auto &rhs) {     \
                        return name == rhs->name();                                                    \
                    });                                                                                \
                    if (res == obj_map.end())                                                          \
                        return nullptr;                                                                \
                    return std::reinterpret_pointer_cast<T>(*res);                                     \
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
            OBJECT_SEARCH(session, sessions)
            OBJECT_SEARCH(timer, timers)

            #undef OBJECT_SEARCH

            default:
                break;
            }

            return nullptr;
        }

        /*! \brief Get kernel object by name
        */
        template <typename T>
        constexpr std::shared_ptr<T> get_by_name(const std::string &name) {
            SYNCHRONIZE_ACCESS;

            constexpr kernel::object_type obj_type = get_object_type<T>();
            return get_by_name_and_type<T>(name, obj_type);
        }

        /*! \brief Get kernel object by ID
        */
        template <typename T>
        constexpr std::shared_ptr<T> get_by_id(const kernel::uid uid) {
            SYNCHRONIZE_ACCESS;

            constexpr kernel::object_type obj_type = get_object_type<T>();
            
            switch (obj_type) {
                // It's gurantee that object are sorted by unique id, by just adding it
                // Deletion will not affect that fact.
            #define OBJECT_SEARCH(obj_type, obj_map)                                                                                \
                case kernel::object_type::obj_type: {                                                                               \
                    auto res = std::lower_bound(obj_map.begin(), obj_map.end(), nullptr, [&](const auto &lhs, const auto &rhs) {    \
                        return lhs->unique_id() < uid;                                                                              \
                    });                                                                                                             \
                    if (res != obj_map.end()) {                                                                                     \
                        return std::reinterpret_pointer_cast<T>(*res);                                                              \
                    }                                                                                                               \
                    return nullptr;                                                                                                 \
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
            OBJECT_SEARCH(session, sessions)
            OBJECT_SEARCH(timer, timers)

            #undef OBJECT_SEARCH

            default:
                break;
            }

            return nullptr;
        }

        /*! \brief Create and add to object array.
        */
        template <typename T, typename ...args>
        constexpr std::shared_ptr<T> create(args... creation_arg) {
            constexpr kernel::object_type obj_type = get_object_type<T>();
            std::shared_ptr<T> obj;

            if constexpr(obj_type == kernel::object_type::server) {
                obj = std::make_shared<T>(sys, creation_arg...);
            } else {
                obj = std::make_shared<T>(this, creation_arg...);
            }

            const kernel::uid obj_uid = obj->unique_id();

            switch (obj_type) {
            case kernel::object_type::thread: {
                threads.push_back(std::move(std::reinterpret_pointer_cast<kernel::thread>(obj)));
                return std::reinterpret_pointer_cast<T>(threads.back());
            }

            case kernel::object_type::process: {
                setup_new_process(std::reinterpret_pointer_cast<kernel::process>(obj));

                processes.push_back(std::move(std::reinterpret_pointer_cast<kernel::process>(obj)));
                return std::reinterpret_pointer_cast<T>(processes.back());
            }

            case kernel::object_type::chunk: {
                chunks.push_back(std::move(std::reinterpret_pointer_cast<kernel::chunk>(obj)));
                return std::reinterpret_pointer_cast<T>(chunks.back());
            }

            case kernel::object_type::server: {
                servers.push_back(std::move(std::reinterpret_pointer_cast<service::server>(obj)));
                return std::reinterpret_pointer_cast<T>(servers.back());
            }

            case kernel::object_type::prop: {
                props.push_back(std::move(std::reinterpret_pointer_cast<service::property>(obj)));
                return std::reinterpret_pointer_cast<T>(props.back());
            }

            case kernel::object_type::session: {
                sessions.push_back(std::move(std::reinterpret_pointer_cast<service::session>(obj)));
                return std::reinterpret_pointer_cast<T>(sessions.back());
            }

            case kernel::object_type::library: {
                libraries.push_back(std::move(std::reinterpret_pointer_cast<kernel::library>(obj)));
                return std::reinterpret_pointer_cast<T>(libraries.back());
            }

            case kernel::object_type::timer: {
                timers.push_back(std::move(std::reinterpret_pointer_cast<kernel::timer>(obj)));
                return std::reinterpret_pointer_cast<T>(timers.back());
            }
            
            case kernel::object_type::mutex: {
                mutexes.push_back(std::move(std::reinterpret_pointer_cast<kernel::mutex>(obj)));
                return std::reinterpret_pointer_cast<T>(mutexes.back());
            }
            
            case kernel::object_type::sema: {
                semas.push_back(std::move(std::reinterpret_pointer_cast<kernel::semaphore>(obj)));
                return std::reinterpret_pointer_cast<T>(semas.back());
            }

            case kernel::object_type::change_notifier: {
                change_notifiers.push_back(std::move(std::reinterpret_pointer_cast<kernel::change_notifier>(obj)));
                return std::reinterpret_pointer_cast<T>(change_notifiers.back());
            }
            
            case kernel::object_type::codeseg: {
                codesegs.push_back(std::move(std::reinterpret_pointer_cast<kernel::codeseg>(obj)));
                return std::reinterpret_pointer_cast<T>(codesegs.back());
            }

            default:
                break;
            }

            return nullptr;
        }

        template <typename T, typename ...args>
        std::pair<kernel::handle, std::shared_ptr<T>> create_and_add(kernel::owner_type owner, 
            args... creation_args) {
            std::shared_ptr<T> obj = create<T>(creation_args...);
            return std::make_pair(open_handle(obj, owner), obj);
        }

        template <typename T, typename ...args>
        std::pair<kernel::handle, std::shared_ptr<T>> create_and_add_thread(kernel::owner_type owner,
            thread_ptr thr, args... creation_args) {
            std::shared_ptr<T> obj = create<T>(creation_args...);
            return std::make_pair(open_handle_with_thread(thr, obj, owner), obj);
        }
    };
}
