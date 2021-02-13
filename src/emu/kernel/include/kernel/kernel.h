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

#include <kernel/legacy/mutex.h>
#include <kernel/legacy/sema.h>
#include <kernel/common.h>
#include <kernel/btrace.h>
#include <kernel/change_notifier.h>
#include <kernel/chunk.h>
#include <kernel/codeseg.h>
#include <kernel/kernel_obj.h>
#include <kernel/ldd.h>
#include <kernel/library.h>
#include <kernel/libmanager.h>
#include <kernel/msgqueue.h>
#include <kernel/mutex.h>
#include <kernel/object_ix.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/sema.h>
#include <kernel/timer.h>
#include <kernel/undertaker.h>

#include <kernel/property.h>
#include <kernel/server.h>
#include <kernel/session.h>

#include <common/types.h>
#include <common/container.h>
#include <common/hash.h>
#include <common/wildcard.h>

#include <kernel/ipc.h>
#include <mem/ptr.h>

#include <cpu/arm_analyser.h>

#include <atomic>
#include <exception>
#include <functional>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <regex>

namespace eka2l1 {
#define SYNCHRONIZE_ACCESS const std::lock_guard<std::mutex> guard(kern_lock)

    class posix_server;
    class ntimer;
    class memory_system;
    class io_system;
    class system;
    class disasm;

    namespace kernel {
        class thread;

        using uid = std::uint64_t;
    }

    namespace config {
        struct state;
        class app_settings;
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
    using mutex_legacy_ptr = kernel::legacy::mutex*;
    using sema_legacy_ptr = kernel::legacy::semaphore*;
    using undertaker_ptr = kernel::undertaker*;

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
        } else if constexpr ((std::is_same_v<T, kernel::mutex>) || (std::is_same_v<T, kernel::legacy::mutex>)) {
            return kernel::object_type::mutex;
        } else if constexpr ((std::is_same_v<T, kernel::semaphore>) || (std::is_same_v<T, kernel::legacy::semaphore>)) {
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
        } else if constexpr (std::is_same_v<T, ldd::factory>) {
            return kernel::object_type::logical_device;
        } else if constexpr (std::is_same_v<T, ldd::channel>) {
            return kernel::object_type::logical_channel;
        } else if constexpr (std::is_same_v<T, kernel::undertaker>) {
            return kernel::object_type::undertaker;
        } else {
            throw std::runtime_error("Unknown kernel object type. Make sure to add new type here");
            return kernel::object_type::unk;
        }
    }

    static constexpr std::uint32_t FIND_HANDLE_IDX_MASK = 0xFFFFFFF;
    static constexpr std::uint32_t FIND_HANDLE_OBJ_TYPE_MASK = 0xF0000000;
    static constexpr std::uint32_t FIND_HANDLE_OBJ_TYPE_SHIFT = 28;

    struct find_handle {
        std::uint32_t index;                  ///< Index of the object in the separate object container.
                                    ///< On EKA2L1 this index starts from 1.
        std::uint64_t object_id;    ///< The ID of the kernel object.
        kernel_obj_ptr obj;         ///< The corresponded kernel object found.
    };

    namespace arm {
        class core;
    }

    namespace common {
        class chunkyseri;
    }

    namespace loader {
        struct rom;
    }

    /**
     * @brief Callback invoked by the kernel when an IPC messages are bout to be sent.
     * 
     * @param server_name       Name of the server this message is sent to.
     * @param ord               The opcode number of this message.
     * @param args              Arguments for this message.
     * @param reqstsaddr        Address of the request status.
     * @param callee            Thread that sent this message.
     */
    using ipc_send_callback = std::function<void(const std::string&, const int, const ipc_arg&, address, kernel::thread*)>;

    /**
     * @brief Callback invoked by the kernel when an IPC message completes.
     * 
     * @param msg               Pointer to the message that being completed.
     * @param complete_code     The code that used to complete this message.
     */
    using ipc_complete_callback = std::function<void(ipc_msg*, const std::int32_t)>;

    /**
     * @brief Callback invoked by the kernel when a thread is killed.
     * 
     * @param thread            Pointer to the thread being killed.
     * @param category          The category of the kill.
     * @param reason            The reason for this thread being killed.
     */
    using thread_kill_callback = std::function<void(kernel::thread*, const std::string&, const std::int32_t)>;

    /**
     * @brief Callback invoked when a breakpoint is hit.
     * 
     * @param core              The CPU core which is currently executing this breakpoint.
     * @param thread            Pointer to the thread that the breakpoint is triggered on.
     * @param addr              Address of the breakpoint.
     */
    using breakpoint_callback = std::function<void(arm::core*, kernel::thread*, const std::uint32_t)>;

    /**
     * @brief Callback invoked when a process switch happens on a core scheduler.
     * 
     * @param core              The CPU core which process switching is currently happening,
     * @param old               The process bout to be switched.
     * @param new               The new process to switched to.
     */
    using process_switch_callback = std::function<void(arm::core*, kernel::process*, kernel::process*)>;

    /**
     * @brief Callback invoked when a codeseg is loaded.
     * 
     * @param name      The name of the codeseg.
     * @param process   The owner process of the codeseg.
     * @param csptr     Pointer to the target codeseg.
     */
    using codeseg_loaded_callback = std::function<void(const std::string&, kernel::process*, codeseg_ptr)>;

    /**
     * @brief Callback invoked when Instruction Memory Barrier is called.
     * 
     * @param process       The process that call the flush.
     * @param address       The address that needs to use the IMB
     * @param size          The number of bytes to flush.
     */
    using imb_range_callback = std::function<void(kernel::process*, address, const std::size_t)>;

    /**
     * @brief Callback invoked when an LDD is requested to be loaded.
     * 
     * @param name          Name of the LDD. 
     */
    using ldd_factory_request_callback = std::function<ldd::factory_instantiate_func(const char*)>;

    /**
     * @brief Callback when a process's UID changes.
     * 
     * @param process       Pointer to the process that changes their UID.
     * @param old_uid       The previous UID types.
     */
    using uid_of_process_change_callback = std::function<void(kernel::process*, kernel::process_uid_type)>;

    struct kernel_global_data {
        kernel::char_set char_set_;

        void reset();
    };

    class kernel_system {
    private:
        friend class debugger_base;
        friend class imgui_debugger;
        friend class gdbstub;
        friend class kernel::process;

        std::array<ipc_msg_ptr, 0x1000> msgs_;
        std::mutex kern_lock_;

        std::vector<kernel_obj_unq_ptr> threads_;
        std::vector<kernel_obj_unq_ptr> processes_;
        std::vector<kernel_obj_unq_ptr> servers_;
        std::vector<kernel_obj_unq_ptr> sessions_;
        std::vector<kernel_obj_unq_ptr> props_;
        std::vector<kernel_obj_unq_ptr> prop_refs_;
        std::vector<kernel_obj_unq_ptr> chunks_;
        std::vector<kernel_obj_unq_ptr> mutexes_;
        std::vector<kernel_obj_unq_ptr> semas_;
        std::vector<kernel_obj_unq_ptr> change_notifiers_;
        std::vector<kernel_obj_unq_ptr> libraries_;
        std::vector<kernel_obj_unq_ptr> codesegs_;
        std::vector<kernel_obj_unq_ptr> timers_;
        std::vector<kernel_obj_unq_ptr> message_queues_;
        std::vector<kernel_obj_unq_ptr> logical_devices_;
        std::vector<kernel_obj_unq_ptr> logical_channels_;
        std::vector<kernel_obj_unq_ptr> undertakers_;

        std::unique_ptr<kernel::btrace> btrace_inst_;
        std::unique_ptr<hle::lib_manager> lib_mngr_;
        std::unique_ptr<kernel::thread_scheduler> thr_sch_;

        ntimer *timing_;
        memory_system *mem_;
        io_system *io_;
        system *sys_;
        config::state *conf_;
        config::app_settings *app_settings_;
        disasm *disassembler_;

        arm::core *cpu_;
        loader::rom *rom_info_;

        //! Handles for some globally shared processes
        kernel::object_ix kernel_handles_;
        int realtime_ipc_signal_evt_;

        mutable std::atomic<kernel::uid> uid_counter_;
        void *rom_map_;
        std::uint64_t base_time_;

        epocver kern_ver_;
        language lang_;

        std::unique_ptr<std::locale> locale_;
        chunk_ptr global_data_chunk_;

        common::identity_container<ipc_send_callback> ipc_send_callbacks_;
        common::identity_container<ipc_complete_callback> ipc_complete_callbacks_;
        common::identity_container<thread_kill_callback> thread_kill_callbacks_;
        common::identity_container<breakpoint_callback> breakpoint_callbacks_;
        common::identity_container<process_switch_callback> process_switch_callback_funcs_;
        common::identity_container<codeseg_loaded_callback> codeseg_loaded_callback_funcs_;
        common::identity_container<imb_range_callback> imb_range_callback_funcs_;
        common::identity_container<ldd_factory_request_callback> ldd_factory_req_callback_funcs_;
        common::identity_container<uid_of_process_change_callback> uid_of_process_callback_funcs_;

        std::unique_ptr<arm::arm_analyser> analyser_;

        using cache_interpreter_func = std::function<bool(arm::core *)>;
        std::map<std::uint32_t, cache_interpreter_func> cache_inters_;

        kernel::chunk* dll_global_data_chunk_;
        std::map<address, std::uint64_t> dll_global_data_offset_;
        std::uint32_t dll_global_data_last_offset_;
        
        std::uint64_t inactivity_starts_;
        kernel::process *nanokern_pr_;

        kernel::chunk *custom_code_chunk;
        address exception_handler_guard_;

    protected:
        void setup_new_process(process_ptr pr);
        void setup_nanokern_controller();
        void setup_custom_code();

        bool cpu_exception_handle_unpredictable(arm::core *core, const address occurred);
        void cpu_exception_thread_handle(arm::core *core);

    public:
        explicit kernel_system(system *esys, ntimer *timing, io_system *io_sys, config::state *conf,
            config::app_settings *settings, loader::rom *rom_info, arm::core *cpu, disasm *diassembler);

        ~kernel_system();

        void wipeout();
        void reset();

        kernel::thread_scheduler *get_thread_scheduler() {
            return thr_sch_.get();
        }

        void cpu_exception_handler(arm::core *core, arm::exception_type exception_type, const std::uint32_t exception_data);

        void call_ipc_send_callbacks(const std::string &server_name, const int ord, const ipc_arg &args,
            address reqsts_addr, kernel::thread *callee);

        void call_ipc_complete_callbacks(ipc_msg *msg, const int complete_code);
        void call_thread_kill_callbacks(kernel::thread *target, const std::string &category, const std::int32_t reason);
        void call_process_switch_callbacks(arm::core *run_core, kernel::process *old, kernel::process *new_one);
        void run_codeseg_loaded_callback(const std::string &lib_name, kernel::process *attacher, codeseg_ptr target);
        void run_imb_range_callback(kernel::process *caller, address range_addr, const std::size_t range_size);
        void run_uid_of_process_change_callback(kernel::process *aff, kernel::process_uid_type type);

        std::size_t register_ipc_send_callback(ipc_send_callback callback);
        std::size_t register_ipc_complete_callback(ipc_complete_callback callback);
        std::size_t register_thread_kill_callback(thread_kill_callback callback);
        std::size_t register_breakpoint_hit_callback(breakpoint_callback callback);
        std::size_t register_process_switch_callback(process_switch_callback callback);
        std::size_t register_codeseg_loaded_callback(codeseg_loaded_callback callback);
        std::size_t register_imb_range_callback(imb_range_callback callback);
        std::size_t register_ldd_factory_request_callback(ldd_factory_request_callback callback);
        std::size_t register_uid_process_change_callback(uid_of_process_change_callback callback);
        
        bool unregister_codeseg_loaded_callback(const std::size_t handle);
        bool unregister_ipc_send_callback(const std::size_t handle);
        bool unregister_ipc_complete_callback(const std::size_t handle);
        bool unregister_thread_kill_callback(const std::size_t handle);
        bool unregister_breakpoint_hit_callback(const std::size_t handle);
        bool unregister_process_switch_callback(const std::size_t handle);
        bool unregister_imb_range_callback(const std::size_t handle);
        bool unregister_ldd_factory_request_callback(const std::size_t handle);
        bool unregister_uid_of_process_change_callback(const std::size_t handle);

        ldd::factory_instantiate_func suitable_ldd_instantiate_func(const char *name);

        kernel::uid next_uid() const;
        std::uint64_t home_time();
        void set_base_time(std::uint64_t time);

        void reschedule();
        void unschedule_wakeup();
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

            servers_.push_back(std::move(svr));
        }

        bool destroy(kernel_obj_ptr obj);
        int close(kernel::handle handle);
        bool get_info(kernel_obj_ptr obj, kernel::handle_info &info);

        kernel_obj_ptr get_kernel_obj_raw(kernel::handle handle, kernel::thread *target);

        bool notify_prop(prop_ident_pair ident);
        bool subscribe_prop(prop_ident_pair ident, int *request_sts);
        bool unsubscribe_prop(prop_ident_pair ident);

        property_ptr get_prop(int category, int key); // Get property by category and key
        property_ptr delete_prop(int category, int key);

        void complete_undertakers(kernel::thread *literally_dies);

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

        bool map_rom(const mem::vm_address addr, const std::string &path);

        epocver get_epoc_version() const {
            return kern_ver_;
        }

        bool is_eka1() const {
            return kern_ver_ < epocver::eka2;
        }

        // For user-provided EPOC version
        void set_epoc_version(const epocver ver);

        void install_memory(memory_system *mem);

        eka2l1::ptr<kernel_global_data> get_global_user_data_pointer();

        /**
         * @brief Get the currently active CPU.
         */
        arm::core *get_cpu();

        int get_ipc_realtime_signal_event() const {
            return realtime_ipc_signal_evt_;
        }

        ntimer *get_ntimer() {
            return timing_;
        }

        memory_system *get_memory_system() {
            return mem_;
        }

        hle::lib_manager *get_lib_manager() {
            return lib_mngr_.get();
        }

        config::state *get_config() {
            return conf_;
        }

        config::app_settings *get_app_settings() {
            return app_settings_;
        }

        system *get_system() {
            return sys_;
        }

        kernel::btrace *get_btrace() {
            return btrace_inst_.get();
        }

        loader::rom *get_rom_info() {
            return rom_info_;
        }

        std::locale *get_current_locale() {
            return locale_.get();
        }

        language get_current_language() const {
            return lang_;
        }

        void set_current_language(const language new_lang);

        // Expose for scripting, indeed very dirty
        std::vector<kernel_obj_unq_ptr> &get_process_list() {
            return processes_;
        }

        std::vector<kernel_obj_unq_ptr> &get_thread_list() {
            return threads_;
        }

        std::vector<kernel_obj_unq_ptr> &get_codeseg_list() {
            return codesegs_;
        }

        /*! \brief Get kernel object by handle
        */
        template <typename T>
        T *get(const kernel::handle handle) {
            T *result = reinterpret_cast<T *>(get_kernel_obj_raw(handle, crr_thread()));

            if (!result) {
                return nullptr;
            }

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
            std::string the_full_name;                                                 \
            rhs->full_name(the_full_name);                                             \
            return the_full_name == name;                                              \
        });                                                                            \
        if (res == obj_map.end())                                                      \
            return nullptr;                                                            \
        return reinterpret_cast<T *>(res->get());                                      \
    }

                OBJECT_SEARCH(mutex, mutexes_)
                OBJECT_SEARCH(sema, semas_)
                OBJECT_SEARCH(chunk, chunks_)
                OBJECT_SEARCH(thread, threads_)
                OBJECT_SEARCH(process, processes_)
                OBJECT_SEARCH(change_notifier, change_notifiers_)
                OBJECT_SEARCH(library, libraries_)
                OBJECT_SEARCH(codeseg, codesegs_)
                OBJECT_SEARCH(server, servers_)
                OBJECT_SEARCH(prop, props_)
                OBJECT_SEARCH(prop_ref, prop_refs_)
                OBJECT_SEARCH(session, sessions_)
                OBJECT_SEARCH(timer, timers_)
                OBJECT_SEARCH(msg_queue, message_queues_)
                OBJECT_SEARCH(logical_device, logical_devices_)
                OBJECT_SEARCH(logical_channel, logical_channels_)
                OBJECT_SEARCH(undertaker, undertakers_)

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

                OBJECT_SEARCH(mutex, mutexes_)
                OBJECT_SEARCH(sema, semas_)
                OBJECT_SEARCH(chunk, chunks_)
                OBJECT_SEARCH(thread, threads_)
                OBJECT_SEARCH(process, processes_)
                OBJECT_SEARCH(change_notifier, change_notifiers_)
                OBJECT_SEARCH(library, libraries_)
                OBJECT_SEARCH(codeseg, codesegs_)
                OBJECT_SEARCH(server, servers_)
                OBJECT_SEARCH(prop, props_)
                OBJECT_SEARCH(prop_ref, prop_refs_)
                OBJECT_SEARCH(session, sessions_)
                OBJECT_SEARCH(timer, timers_)
                OBJECT_SEARCH(msg_queue, message_queues_)
                OBJECT_SEARCH(logical_device, logical_devices_)
                OBJECT_SEARCH(logical_channel, logical_channels_)
                OBJECT_SEARCH(undertaker, undertakers_)

#undef OBJECT_SEARCH

            default:
                break;
            }

            return nullptr;
        }

        template <typename T>
        T *add_object(std::unique_ptr<T> &obj) {
            constexpr kernel::object_type obj_type = get_object_type<T>();
            
#define ADD_OBJECT_TO_CONTAINER(type, container, additional_setup) \
    case type:                                                     \
        additional_setup;                                          \
        container.push_back(std::move(obj));                       \
        return reinterpret_cast<T *>(container.back().get());

            switch (obj_type) {
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::thread, threads_, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::process, processes_, setup_new_process(reinterpret_cast<process_ptr>(obj.get())));
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::chunk, chunks_, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::server, servers_, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::prop, props_, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::prop_ref, prop_refs_, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::session, sessions_, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::library, libraries_, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::timer, timers_, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::mutex, mutexes_, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::sema, semas_, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::change_notifier, change_notifiers_, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::codeseg, codesegs_, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::msg_queue, message_queues_, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::logical_device, logical_devices_, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::logical_channel, logical_channels_, )
                ADD_OBJECT_TO_CONTAINER(kernel::object_type::undertaker, undertakers_, )

            default:
                break;
            }

            return nullptr;

#undef ADD_OBJECT_TO_CONTAINER
        }

        /*! \brief Create and add to object array.
        */
        template <typename T, typename... args>
        T *create(args... creation_arg) {
            constexpr kernel::object_type obj_type = get_object_type<T>();
            std::unique_ptr<T> obj = std::make_unique<T>(this, creation_arg...);

            return add_object<T>(obj);
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
        
        kernel_obj_ptr get_object_from_find_handle(const std::uint32_t find_handle);

        // Lock the kernel
        void lock() {
            kern_lock_.lock();
        }

        // Unlock the kernel
        void unlock() {
            kern_lock_.unlock();
        }

        void stop_cores_idling();
        bool should_core_idle_when_inactive();

        address get_global_dll_space(const address handle, std::uint8_t **data_ptr = nullptr, std::uint32_t *size_of_data = nullptr);
        bool allocate_global_dll_space(const address handle, const std::uint32_t size, address &data_ptr_guest, std::uint8_t **data_ptr_host = nullptr);

        address get_exception_handler_guard();

        /**
         * @brief Get the inactivity time in seconds.
         * 
         * Inactivity time represents the amount of duration which this device has not received any physical
         * input events.
         * 
         * @returns Inactivity time in seconds.
         */
        std::uint32_t inactivity_time();

        /**
         * @brief Reset the inactivity timer.
         * 
         * This will count as the device has received a fake physical input. And the timer
         * will be reseted.
         */
        void reset_inactivity_time();

        /**
         * @brief Start the bootload procedures.
         * 
         * The function setups kernel components like real phone.
         * 
         * This include spawning neccessary proccesses.
         */
        void start_bootload();
    };
}
