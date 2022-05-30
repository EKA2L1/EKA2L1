/*
 * Copyright (c) 2018 EKA2L1 Team
 *
 * This file is part of EKA2L1 project.
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

#include <common/types.h>
#include <common/watcher.h>
#include <common/container.h>

#include <scripting/lua_helper.h>
#include <scripting/platform.h>

#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

namespace eka2l1 {
    class system;
    struct ipc_msg;

    namespace kernel {
        class thread;
        class process;
        class codeseg;
    }

    using codeseg_ptr = kernel::codeseg *;

    namespace arm {
        class core;
    }

    namespace scripting {
        class thread;
        class ipc_message_wrapper;
    }
}

namespace eka2l1::manager {
    typedef void(__stdcall *ipc_sent_func)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
        std::uint32_t, std::uint32_t, eka2l1::scripting::thread *);
    typedef void(__stdcall *ipc_completed_func)(eka2l1::scripting::ipc_message_wrapper *);
    typedef void(__stdcall *breakpoint_hit_func)();

    struct script_module;

    struct script_function {
        enum meta_category {
            META_CATEGORY_IPC = 0,
            META_CATEGORY_PENDING_PATCH_BREAKPOINT = 1,
            META_CATEGORY_BREAKPOINT = 2
        };

        std::shared_ptr<script_module> parent_; 

        void* func_;
        meta_category category_;

        explicit script_function(std::shared_ptr<script_module> parent, void *func, meta_category category)
            : parent_(parent)
            , func_(func)
            , category_(category) {
        }

        template <typename T>
        T cast() {
            return reinterpret_cast<T>(func_);
        }
    };
    
    struct script_module {
        scripting::luacpp_state state_;
        common::identity_container<std::unique_ptr<script_function>> functions_;

        explicit script_module(lua_State *state)
            : state_(state) {
        }

        lua_State *lua_state() {
            return state_.state_;
        }
    };

    struct breakpoint_info {
        std::string lib_name_;
        std::uint32_t addr_;

        enum {
            FLAG_IS_ORDINAL = 1 << 0,
            FLAG_BASED_IMAGE = 1 << 1
        };

        std::uint8_t flags_;
        std::uint32_t attached_process_;
        std::uint32_t codeseg_uid3_;
        std::uint32_t codeseg_hash_;

        script_function *invoke_;

        explicit breakpoint_info();
    };

    using ipc_operation_func_list = std::vector<script_function*>;
    using breakpoint_info_list = std::vector<breakpoint_info>;

    struct breakpoint_info_list_record {
        breakpoint_info_list list_;
        std::map<std::uint32_t, std::uint32_t> source_insts_;
    };

    static constexpr std::uint32_t INVALID_HOOK_HANDLE = 0xFFFFFFFF;

    /**
     * \brief A manager for all custom Python scripts of EKA2L1 
     *
     * This class manages all Python modules of an EKA2L1 instance.
     * 
     * This class also manages all panic, system call and reschedule hooks,
     * allowing users to discover and work with high-level interface of
     * EKA2L1
     */
    class scripts {
    private:
        // ================= PYTHON SECTION ========================
        std::unordered_map<std::string, std::shared_ptr<script_module>> modules;
        std::unordered_map<std::uint32_t, breakpoint_info_list_record> breakpoints; ///< Breakpoints complete patching
        std::unordered_map<std::string, std::map<std::uint64_t, ipc_operation_func_list>> ipc_functions;

        struct breakpoint_hit_info {
            bool hit_;
            std::uint32_t addr_;
        };

        std::map<std::uint64_t, breakpoint_hit_info> last_breakpoint_script_hits;
        std::shared_ptr<script_module> current_module;

        breakpoint_info_list breakpoint_wait_patch; ///< Breakpoints that still require patching

        std::size_t ipc_send_callback_handle;
        std::size_t ipc_complete_callback_handle;
        std::size_t breakpoint_hit_callback_handle;
        std::size_t process_switch_callback_handle;
        std::size_t codeseg_loaded_callback_handle;
        std::size_t uid_change_callback_handle;
        std::size_t imb_range_callback_handle;

        system *sys;
        std::mutex smutex;

        common::directory_watcher folder_watcher;

    protected:
        bool call_module_entry(const std::string &module);
        bool remove_function_impl(script_function *func);

        /**
         * \brief Patch ordinal breakpoints with address based on code base address of given image.
         * 
         * \param name      The code segment of the library that requires patching.
         */
        void patch_library_hook(const codeseg_ptr &seg);

        /**
         * \brief Patch breakpoints that are not relocated yet.
         * 
         * \param process_uid       The process of the UID that holds the relocated image.
         * \param seg               The code segment that requires patching.
         * \param new_code_addr     The new code base address of the given image.
         */
        void patch_unrelocated_hook(const std::uint32_t process_uid,const codeseg_ptr &seg, const address new_code_addr);


    public:
        explicit scripts(system *sys);
        ~scripts();

        bool import_module(const std::string &path);
        void unload_module(const std::string &path);
        void import_all_modules();

        void handle_breakpoint(arm::core *running_core, kernel::thread *thr_triggered, const std::uint32_t addr);
        bool last_breakpoint_hit(kernel::thread *thr);
        void reset_breakpoint_hit(arm::core *running_core, kernel::thread *thr);

        void handle_codeseg_loaded(const std::string &name, kernel::process *attacher, codeseg_ptr target);
        void handle_process_switch(arm::core *core_switch, kernel::process *old_friend, kernel::process *new_friend);
        void handle_uid_process_change(kernel::process *aff, const std::uint32_t old_one);
        void handle_imb_range(kernel::process *p, const address addr, const std::size_t ss);

        void call_ipc_send(const std::string &server_name, const int opcode, const std::uint32_t arg0,
            const std::uint32_t arg1, const std::uint32_t arg2, const std::uint32_t arg3,
            const std::uint32_t flags, const std::uint32_t reqstsaddr, kernel::thread *callee);
        void call_ipc_complete(const std::string &server_name, const int opcode,
            ipc_msg *msg);

        /**
         * \brief Register a library hook.
         * 
         * \param name              The name of the library we want to hook to.
         * \param ord               The ordinal of the function we want to hook.
         * \param process_uid       The UID of the process we wants to invoke this hook.
         * \param func              The hook.
         */
        std::uint32_t register_library_hook(const std::string &name, const std::uint32_t ord, const std::uint32_t process_uid, const std::uint32_t uid3, const std::uint32_t seghash, breakpoint_hit_func func);
        std::uint32_t register_breakpoint(const std::string &lib_name, const uint32_t addr, const std::uint32_t process_uid, const std::uint32_t uid3, const std::uint32_t seghash, breakpoint_hit_func func);
        std::uint32_t register_ipc(const std::string &server_name, const int opcode, const int invoke_when, void* func);

        bool call_breakpoints(const std::uint32_t addr, const std::uint32_t process_uid);

        /**
         * \brief Set all pending breakpoints.
         * 
         * \param pr    The process we want to attach the breakpoint to.
         */
        void write_breakpoint_blocks(kernel::process *pr);

        /**
         * \brief Set breakpoint on a process, at specified address.
         * 
         * \param pr        The target process we want to attach a breakpoint to
         * \param target    The address of the breakpoint.
         */
        void write_breakpoint_block(kernel::process *pr, const vaddress target);

        /**
         * \brief Write back all instructions that was overwritted by bkpt.
         * 
         * \param pr        The target process we want to give back the instruction.
         */
        void write_back_breakpoints(kernel::process *pr);

        /**
         * \brief Write back instruction that was overwritted by bkpt.
         * 
         * \param pr        The target process we want to give back the instruction.
         * \param target    The address to write back instruction.
         * 
         * \returns True on success and the bkpt was written before.
         */
        bool write_back_breakpoint(kernel::process *pr, const vaddress target);

        script_function *make_function(void *func_ptr, const script_function::meta_category category, std::size_t *handle);
        void remove_function(const std::uint32_t handle);

        template <typename T, typename ...Args>
        void call(script_function *func, Args... args) {
            current_module = func->parent_;
            func->cast<T>()(args...);
            lua_gc(current_module->lua_state(), LUA_GCCOLLECT, 0);
            current_module = nullptr;
        }
    };
}
