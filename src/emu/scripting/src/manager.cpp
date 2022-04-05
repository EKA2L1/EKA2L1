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

#include <common/algorithm.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <common/platform.h>
#include <common/path.h>

#include <scripting/instance.h>
#include <scripting/manager.h>
#include <scripting/message.h>

#include <scripting/thread.h>

#include <kernel/kernel.h>
#include <system/epoc.h>

namespace eka2l1::manager {
    static void script_file_changed_callback(void *data, common::directory_changes &changes) {
        scripts *manager = reinterpret_cast<scripts*>(data);
        for (std::size_t i = 0; i < changes.size(); i++) {
            if (changes[i].filename_.empty()) {
                continue;
            }

            if (!(changes[i].change_ & common::directory_change_action_created))
                manager->unload_module(changes[i].filename_);

            if (!(changes[i].change_ & common::directory_change_action_moved_from) &&
                !(changes[i].change_ & common::directory_change_action_delete))
                manager->import_module("scripts/" + changes[i].filename_);
        }
    }

    breakpoint_info::breakpoint_info()
        : addr_(0)
        , flags_(0)
        , attached_process_(0) {
    }

    scripts::scripts(system *sys)
        : sys(sys)
        , ipc_send_callback_handle(0)
        , ipc_complete_callback_handle(0)
        , breakpoint_hit_callback_handle(0)
        , process_switch_callback_handle(0)
        , codeseg_loaded_callback_handle(0)
        , imb_range_callback_handle(0) {
        scripting::set_current_instance(sys);
    }

    scripts::~scripts() {
        kernel_system *kern = sys->get_kernel_system();

        if (ipc_send_callback_handle)
            kern->unregister_ipc_send_callback(ipc_send_callback_handle);

        if (ipc_complete_callback_handle)
            kern->unregister_ipc_complete_callback(ipc_complete_callback_handle);

        if (breakpoint_hit_callback_handle)
            kern->unregister_breakpoint_hit_callback(breakpoint_hit_callback_handle);

        if (process_switch_callback_handle)
            kern->unregister_process_switch_callback(process_switch_callback_handle);

        if (codeseg_loaded_callback_handle) {
            kern->unregister_codeseg_loaded_callback(codeseg_loaded_callback_handle);
        }

        if (uid_change_callback_handle) {
            kern->unregister_uid_of_process_change_callback(uid_change_callback_handle);
        }

        if (imb_range_callback_handle) {
            kern->unregister_imb_range_callback(imb_range_callback_handle);
        }

        modules.clear();
    }

    script_function *scripts::make_function(void *func_ptr, const script_function::meta_category category, std::size_t *handle) {
        if (!current_module) {
            LOG_ERROR(SCRIPTING, "No module is active to make a function!");
            return nullptr;
        }

        std::unique_ptr<script_function> func = std::make_unique<script_function>(current_module, func_ptr, category);
        script_function *func_data = func.get();

        if (handle) {
            *handle = current_module->functions_.add(func);
        }

        return func_data;
    }

    bool scripts::remove_function_impl(script_function *target_func) {
        if (!target_func) {
            return false;
        }

        switch (target_func->category_) {
        case script_function::META_CATEGORY_PENDING_PATCH_BREAKPOINT:
            for (std::size_t j = 0; j < breakpoint_wait_patch.size(); j++) {
                if (breakpoint_wait_patch[j].invoke_ == target_func) {
                    breakpoint_wait_patch.erase(breakpoint_wait_patch.begin() + j--);
                }
            }

            break;

        case script_function::META_CATEGORY_BREAKPOINT:
            for (auto &breakpoint: breakpoints) {
                for (std::size_t j = 0; j < breakpoint.second.list_.size(); j++) {
                    if (breakpoint.second.list_[j].invoke_ == target_func) {
                        kernel_system *kern = sys->get_kernel_system();
                        if (!breakpoint.second.source_insts_.empty()) {
                            for (auto &process_unq: kern->get_process_list()) {
                                kernel::process *pr = reinterpret_cast<kernel::process*>(process_unq.get());
                                if ((breakpoint.second.list_[j].attached_process_ == 0) || (pr && (breakpoint.second.list_[j].attached_process_ == pr->get_uid()))) {
                                    write_back_breakpoint(pr, breakpoint.second.list_[j].addr_);
                                }

                                if (breakpoint.second.source_insts_.empty()) {
                                    break;
                                }
                            }
                        }

                        breakpoint.second.list_.erase(breakpoint.second.list_.begin() + j--);
                    }
                }
            }

            break;
        
        case script_function::META_CATEGORY_IPC:
            for (auto &opcode_map: ipc_functions) {
                for (auto &[identifier, funcs]: opcode_map.second) {
                    for (std::size_t j = 0; j < funcs.size(); j++) {
                        if (funcs[j] == target_func) {
                            funcs.erase(funcs.begin() + j--);
                        }
                    }
                }
            }

        default:
            LOG_WARN(SCRIPTING, "Script function is not in a recognisable category, no removal!");
            return false;
        }

        return true;
    }

    void scripts::remove_function(const std::uint32_t handle) {
        if (!current_module) {
            LOG_ERROR(SCRIPTING, "No module is active to remove a function!");
            return;
        }

        std::unique_ptr<script_function> *func_ptr = current_module->functions_.get(static_cast<const std::size_t>(handle));
        if (!func_ptr) {
            LOG_ERROR(SCRIPTING, "No function found with handle {}", handle);
            return;
        }

        if (remove_function_impl(func_ptr->get())) {
            current_module->functions_.remove(static_cast<const std::size_t>(handle));
        }
    }

    void scripts::import_all_modules() {
        // Import all scripts
        std::string cur_dir;
        common::get_current_directory(cur_dir);

        auto scripts_dir = common::make_directory_iterator("scripts/");
        if (!scripts_dir) {
            return;
        }
        scripts_dir->detail = true;

        common::dir_entry scripts_entry;

        while (scripts_dir->next_entry(scripts_entry) == 0) {
            const std::string ext = path_extension(scripts_entry.name);

            if ((scripts_entry.type == common::FILE_REGULAR) && (ext == ".lua")) {
                auto module_name = filename(scripts_entry.name);
                import_module("scripts/" + module_name);
            }
        }

        common::set_current_directory(cur_dir);

        // Listen for script change
        folder_watcher.watch("scripts/", script_file_changed_callback, this, common::directory_change_move | common::directory_change_creation
            | common::directory_change_last_write);
    }

    bool scripts::import_module(const std::string &path) {
        const std::string name_full = eka2l1::filename(path);
        const std::string name = eka2l1::replace_extension(name_full, "");

        if (modules.find(name) == modules.end()) {
            std::string crr_path;
            if (!common::get_current_directory(crr_path)) {
                LOG_ERROR(SCRIPTING, "Unable to get current directory!");
                return false;
            }

            const std::string &pr_path = eka2l1::absolute_path(eka2l1::file_directory(path), crr_path);
            std::lock_guard<std::mutex> guard(smutex);

            if (!common::set_current_directory(pr_path)) {
                LOG_ERROR(SCRIPTING, "Fail to set current directory to script folder!");
                return false;
            }

            if (eka2l1::path_extension(path) == ".lua") {
                lua_State *new_state = luaL_newstate();
                luaL_openlibs(new_state);
                luaL_dostring(new_state, "package.path = package.path .. ';scripts/?.lua'");

                common::ro_std_file_stream script_stream(name_full, true);
                std::string script_content(script_stream.size(), '0');

                script_stream.read(script_content.data(), script_content.size());

                if (luaL_loadbuffer(new_state, script_content.data(), script_content.size(), name.c_str()) == LUA_OK) {
                    modules.emplace(name.c_str(), std::make_shared<script_module>(new_state));
                } else {
                    LOG_WARN(SCRIPTING, "Fail to load script {}, error {}", name, lua_tostring(new_state, -1));
                    lua_close(new_state);
                }
            }

            if (!common::set_current_directory(crr_path)) {
                LOG_WARN(SCRIPTING, "Can't restore the previous current directory!");
            }

            if (!call_module_entry(name.c_str())) {
                // If the module entry failed, we still success, but not execute any futher method
                return true;
            }

            LOG_TRACE(SCRIPTING, "Module {} loaded!", name);
        }

        return true;
    }

    void scripts::unload_module(const std::string &path) {
        const std::string name_full = eka2l1::filename(path);
        const std::string name = eka2l1::replace_extension(name_full, "");

        auto find_result = modules.find(name);
        if (find_result != modules.end()) {
            std::shared_ptr<script_module> module = find_result->second;
            for (auto &function: module->functions_) {
                // Delete all references in breakpoint map and IPC map
                remove_function_impl(function.get());
            }

            if (current_module == find_result->second) {
                current_module = nullptr;
            }

            modules.erase(find_result);
            LOG_TRACE(SCRIPTING, "Module {} unloaded!", name);
        }
    }

    bool scripts::call_module_entry(const std::string &module) {
        if (!ipc_send_callback_handle) {
            kernel_system *kern = sys->get_kernel_system();

            ipc_send_callback_handle = kern->register_ipc_send_callback([this](const std::string &svr_name, const int ord, const ipc_arg &args, address reqstsaddr, kernel::thread *callee) {
                call_ipc_send(svr_name, ord, args.args[0], args.args[1], args.args[2], args.args[3], args.flag, reqstsaddr, callee);
            });

            ipc_complete_callback_handle = kern->register_ipc_complete_callback([this](ipc_msg *msg, const std::int32_t complete_code) {
                if (msg->msg_session)
                    call_ipc_complete(msg->msg_session->get_server()->name(), msg->function, msg);
            });

            breakpoint_hit_callback_handle = kern->register_breakpoint_hit_callback([this](arm::core *core, kernel::thread *correspond, const vaddress addr) {
                handle_breakpoint(core, correspond, addr);
            });

            process_switch_callback_handle = kern->register_process_switch_callback([this](arm::core *core, kernel::process *old_one, kernel::process *new_one) {
                handle_process_switch(core, old_one, new_one);
            });

            codeseg_loaded_callback_handle = kern->register_codeseg_loaded_callback([this](const std::string &name, kernel::process *attacher, codeseg_ptr target) {
                handle_codeseg_loaded(name, attacher, target);
            });

            uid_change_callback_handle = kern->register_uid_process_change_callback([this](kernel::process *aff, kernel::process_uid_type type) {
                handle_uid_process_change(aff, std::get<2>(type));
            });

            imb_range_callback_handle = kern->register_imb_range_callback([this](kernel::process *pr, const address addr, const std::size_t size) {
                handle_imb_range(pr, addr, size);
            });
        }

        if (modules.find(module) == modules.end()) {
            return false;
        }

        std::shared_ptr<script_module> &state = modules[module];
        current_module = state;

        if (lua_pcall(state->lua_state(), 0, 0, 0) != LUA_OK) {
            LOG_ERROR(SCRIPTING, "Error executing script entry of {}: {}", module, lua_tostring(state->lua_state(), -1));            
            return false;
        }

        return true;
    }

    std::uint32_t scripts::register_ipc(const std::string &server_name, const int opcode, const int invoke_when, void* func) {
        std::size_t handle = 0;
        script_function *managed_func = make_function(func, script_function::META_CATEGORY_IPC, &handle);

        if (!managed_func) {
            return INVALID_HOOK_HANDLE;
        }

        ipc_functions[server_name][(static_cast<std::uint64_t>(opcode) | (static_cast<std::uint64_t>(invoke_when) << 32))].push_back(managed_func);
        return static_cast<std::uint32_t>(handle);
    }

    void scripts::write_breakpoint_block(kernel::process *pr, const vaddress target) {
        const vaddress aligned = target & ~1;

        if (!pr) {
            return;
        }

        std::uint32_t *data = reinterpret_cast<std::uint32_t *>(pr->get_ptr_on_addr_space(aligned));
        if (!data) {
            return;
        }

        auto ite = std::find_if(breakpoints[aligned].list_.begin(), breakpoints[aligned].list_.end(),
            [=](const breakpoint_info &info) { return (info.attached_process_ == 0) || (info.attached_process_ == pr->get_uid()); });

        auto &source_insts = breakpoints[aligned].source_insts_;

        if (ite == breakpoints[aligned].list_.end() || source_insts.find(pr->get_uid()) != source_insts.end() || !data) {
            return;
        }

        source_insts[pr->get_uid()] = data[0];

        if (target & 1) {
            // The target destination is thumb
            *reinterpret_cast<std::uint16_t *>(data) = 0xBE00;
        } else {
            data[0] = 0xE1200070; // bkpt #0
        }

        kernel_system *kern = sys->get_kernel_system();

        // Must clear cache of all cores, but since we only have one core now...
        kern->get_cpu()->imb_range((target & ~1), (target & 1) ? 2 : 4);
    }

    bool scripts::write_back_breakpoint(kernel::process *pr, const vaddress target) {
        auto &sources = breakpoints[target & ~1].source_insts_;
        auto source_value = sources.find(pr->get_uid());

        if (source_value == sources.end()) {
            return false;
        }

        std::uint32_t *data = reinterpret_cast<std::uint32_t *>(pr->get_ptr_on_addr_space(target & ~1));
        if (!data) {
            return false;
        }

        data[0] = source_value->second;

        sources.erase(source_value);
        return true;
    }

    void scripts::write_back_breakpoints(kernel::process *pr) {
        for (const auto &[addr, info] : breakpoints) {
            if (!info.list_.empty()) {
                write_back_breakpoint(pr, info.list_[0].addr_);
            }
        }
    }

    void scripts::write_breakpoint_blocks(kernel::process *pr) {
        const std::lock_guard<std::mutex> guard(smutex);

        for (const auto &[addr, info] : breakpoints) {
            // The address on the info contains information about Thumb/ARM mode
            if (!info.list_.empty())
                write_breakpoint_block(pr, info.list_[0].addr_);
        }
    }

    std::uint32_t scripts::register_library_hook(const std::string &name, const std::uint32_t ord, const std::uint32_t process_uid, const std::uint32_t uid3, breakpoint_hit_func func) {
        const std::string lib_name_lower = common::lowercase_string(name);

        breakpoint_info info;
        std::size_t handle = 0;

        info.lib_name_ = lib_name_lower;
        info.invoke_ = make_function(reinterpret_cast<void*>(func), script_function::META_CATEGORY_PENDING_PATCH_BREAKPOINT, &handle);

        if (!info.invoke_) {
            return INVALID_HOOK_HANDLE;
        }

        info.flags_ = breakpoint_info::FLAG_IS_ORDINAL;
        info.addr_ = ord;
        info.attached_process_ = process_uid;
        info.codeseg_uid3_ = uid3;

        hle::lib_manager *manager = sys->get_lib_manager();
        if (manager) {
            if (codeseg_ptr seg = manager->load(common::utf8_to_ucs2(name))) {
                std::uint32_t uid3_seg = std::get<2>(seg->get_uids());
                if ((uid3_seg == 0) || (uid3_seg == uid3)) {
                    std::vector<kernel::process*> processes = seg->attached_processes();
                    auto find_res = std::find_if(processes.begin(), processes.end(), [process_uid](kernel::process *target) {
                        return (target->get_uid() == process_uid);
                    });

                    if (process_uid != 0) {
                        auto find_res = std::find_if(processes.begin(), processes.end(), [process_uid](kernel::process *target) {
                            return (target->get_uid() == process_uid);
                        });

                        kernel::process *finally = nullptr;
                        if (find_res != processes.end()) {
                            finally = *find_res;
                        }

                        processes.clear();
                        processes.push_back(finally);
                    }

                    for (kernel::process *process: processes) {
                        info.invoke_->category_ = script_function::META_CATEGORY_BREAKPOINT;
                        info.addr_ = seg->lookup(process, info.addr_);
                        info.flags_ = 0;

                        if (info.addr_ == 0) {
                            LOG_ERROR(SCRIPTING, "Ordinal {} does not exist in library {}", ord, name);
                            current_module->functions_.remove(handle);

                            return INVALID_HOOK_HANDLE;
                        }
                    }
                }
            }
        }

        if (info.flags_ & breakpoint_info::FLAG_IS_ORDINAL) {
            breakpoint_wait_patch.push_back(info);
        } else {
            kernel_system *kern = sys->get_kernel_system();
            breakpoints[info.addr_ & ~1].list_.push_back(std::move(info));

            if (kern->crr_process())
                write_breakpoint_block(kern->crr_process(), info.addr_);
        }

        return static_cast<std::uint32_t>(handle);
    }

    std::uint32_t scripts::register_breakpoint(const std::string &lib_name, const uint32_t addr, const std::uint32_t process_uid, const std::uint32_t uid3, breakpoint_hit_func func) {
        const std::string lib_name_lower = common::lowercase_string(lib_name);
        std::size_t handle = 0;

        breakpoint_info info;
        info.lib_name_ = lib_name_lower;
        info.flags_ = breakpoint_info::FLAG_BASED_IMAGE;
        info.addr_ = addr;
        info.invoke_ = make_function(reinterpret_cast<void*>(func), script_function::META_CATEGORY_BREAKPOINT, &handle);
        info.attached_process_ = process_uid;

        if (!info.invoke_) {
            return INVALID_HOOK_HANDLE;
        }

        if (lib_name_lower == "constantaddr") {
            info.flags_ = 0;
            info.codeseg_uid3_ = 0;
        } else {
            hle::lib_manager *manager = sys->get_lib_manager();
            if (manager) {
                if (codeseg_ptr seg = manager->load(common::utf8_to_ucs2(lib_name))) {
                    std::vector<kernel::process*> processes = seg->attached_processes();

                    if (process_uid != 0) {
                        auto find_res = std::find_if(processes.begin(), processes.end(), [process_uid](kernel::process *target) {
                            return (target->get_uid() == process_uid);
                        });

                        kernel::process *finally = nullptr;
                        if (find_res != processes.end()) {
                            finally = *find_res;
                        }

                        processes.clear();
                        processes.push_back(finally);
                    }

                    for (kernel::process *process: processes) {
                        address base = seg->get_code_run_addr(process);

                        if (base == 0) {
                            LOG_ERROR(SCRIPTING, "Can't retrieve code run address of process base {} with UID", process->name(), process_uid);
                        }

                        if (seg->is_rom()) {
                            base = 0;
                        }

                        info.invoke_->category_ = script_function::META_CATEGORY_BREAKPOINT;
                        info.addr_ += base;
                        info.flags_ = 0;
                    }
                }
            }

            info.codeseg_uid3_ = uid3;
        }

        if (info.flags_ & breakpoint_info::FLAG_BASED_IMAGE) {
            breakpoint_wait_patch.push_back(info);
        } else {
            kernel_system *kern = sys->get_kernel_system();

            breakpoints[addr & ~1].list_.push_back(std::move(info));

            if (kern->crr_process())
                write_breakpoint_block(kern->crr_process(), addr);
        }

        return static_cast<std::uint32_t>(handle);
    }

    void scripts::patch_library_hook(const std::string &name, const std::uint32_t uid3, const std::vector<vaddress> &exports) {
        const std::lock_guard<std::mutex> guard(smutex);
        const std::string lib_name_lower = common::lowercase_string(name);

        for (auto &breakpoint : breakpoint_wait_patch) {
            if ((breakpoint.flags_ & breakpoint_info::FLAG_IS_ORDINAL) && (breakpoint.lib_name_ == lib_name_lower) &&
                ((breakpoint.codeseg_uid3_ == uid3) || (breakpoint.codeseg_uid3_ == 0))) {
                breakpoint.addr_ = exports[breakpoint.addr_ - 1];

                // It's now based on image. Only need rebase
                breakpoint.flags_ &= ~breakpoint_info::FLAG_IS_ORDINAL;
                breakpoint.flags_ |= breakpoint_info::FLAG_BASED_IMAGE;
            }
        }
    }

    void scripts::patch_unrelocated_hook(const std::uint32_t process_uid, const std::uint32_t uid3, const std::string &name, const address new_code_addr) {
        const std::lock_guard<std::mutex> guard(smutex);
        const std::string lib_name_lower = common::lowercase_string(name);

        for (breakpoint_info &breakpoint : breakpoint_wait_patch) {
            if (((breakpoint.attached_process_ == 0) || (breakpoint.attached_process_ == process_uid)) && (breakpoint.lib_name_ == lib_name_lower)
                && ((breakpoint.codeseg_uid3_ == uid3) || (breakpoint.codeseg_uid3_ == 0)) && (breakpoint.flags_ & breakpoint_info::FLAG_BASED_IMAGE)) {
                breakpoint_info patched = breakpoint;

                patched.addr_ += new_code_addr;
                patched.flags_ &= ~breakpoint_info::FLAG_BASED_IMAGE;
                patched.invoke_->category_ = script_function::META_CATEGORY_BREAKPOINT;

                breakpoints[patched.addr_ & ~1].list_.push_back(patched);

                kernel_system *kern = sys->get_kernel_system();

                if (kern->crr_process())
                    write_breakpoint_block(kern->crr_process(), patched.addr_);
            }
        }
    }

    void scripts::call_ipc_send(const std::string &server_name, const int opcode, const std::uint32_t arg0, const std::uint32_t arg1,
        const std::uint32_t arg2, const std::uint32_t arg3, const std::uint32_t flags, const std::uint32_t reqsts_addr,
        kernel::thread *callee) {
        const std::lock_guard<std::mutex> guard(smutex);

        eka2l1::system *crr_instance = scripting::get_current_instance();
        eka2l1::scripting::set_current_instance(sys);

        for (auto &ipc_func : ipc_functions[server_name][opcode]) {
            call<ipc_sent_func>(ipc_func, arg0, arg1, arg2, arg3, flags, reqsts_addr, new scripting::thread(reinterpret_cast<std::uint64_t>(callee)));
        }

        scripting::set_current_instance(crr_instance);
    }

    void scripts::call_ipc_complete(const std::string &server_name,
        const int opcode, ipc_msg *msg) {
        std::lock_guard<std::mutex> guard(smutex);

        eka2l1::system *crr_instance = scripting::get_current_instance();
        eka2l1::scripting::set_current_instance(sys);

        for (auto &ipc_func : ipc_functions[server_name][(2ULL << 32) | opcode]) {
            ipc_completed_func comp_func = reinterpret_cast<ipc_completed_func>(ipc_func);
            comp_func(new scripting::ipc_message_wrapper(reinterpret_cast<std::uint64_t>(msg)));
        }

        scripting::set_current_instance(crr_instance);
    }

    bool scripts::call_breakpoints(const std::uint32_t addr, const std::uint32_t process_uid) {
        const std::lock_guard<std::mutex> guard(smutex);
        if (breakpoints.find(addr & ~1) == breakpoints.end()) {
            return false;
        }

        breakpoint_info_list &list = breakpoints[addr & ~1].list_;

        for (auto &info : list) {
            if ((info.attached_process_ != 0) && (info.attached_process_ != process_uid)) {
                continue;
            }

            call<breakpoint_hit_func>(info.invoke_);
        }

        return true;
    }

    bool scripts::last_breakpoint_hit(kernel::thread *thr) {
        if (!thr) {
            return false;
        }

        return last_breakpoint_script_hits[thr->unique_id()].hit_;
    }

    void scripts::reset_breakpoint_hit(arm::core *running_core, kernel::thread *thr) {
        const std::lock_guard<std::mutex> guard(smutex);

        breakpoint_hit_info &info = last_breakpoint_script_hits[thr->unique_id()];
        write_breakpoint_block(thr->owning_process(), info.addr_);

        info.hit_ = false;
    }

    void scripts::handle_breakpoint(arm::core *running_core, kernel::thread *correspond, const std::uint32_t addr) {
        running_core->stop();
        running_core->save_context(correspond->get_thread_context());

        if (!last_breakpoint_script_hits[correspond->unique_id()].hit_) {
            const vaddress cur_addr = addr | ((running_core->get_cpsr() & 0x20) >> 5);

            if (call_breakpoints(cur_addr, correspond->owning_process()->get_uid())) {
                breakpoint_hit_info &info = last_breakpoint_script_hits[correspond->unique_id()];
                info.hit_ = true;
                const std::uint32_t last_breakpoint_script_size_ = (running_core->get_cpsr() & 0x20) ? 2 : 4;
                info.addr_ = cur_addr;

                write_back_breakpoint(correspond->owning_process(), cur_addr);

                correspond->get_thread_context().set_pc(addr);

                running_core->set_pc(addr);
                running_core->imb_range(addr, last_breakpoint_script_size_);
            }
        }
    }

    void scripts::handle_process_switch(arm::core *core_switch, kernel::process *old_friend, kernel::process *new_friend) {
        if (old_friend) {
            write_back_breakpoints(old_friend);
        }

        write_breakpoint_blocks(new_friend);
    }

    void scripts::handle_codeseg_loaded(const std::string &name, kernel::process *attacher, codeseg_ptr target) {
        const std::uint32_t uid3 = std::get<2>(target->get_uids());

        patch_library_hook(name, uid3, target->get_export_table_raw());
        patch_unrelocated_hook(attacher ? (attacher->get_uid()) : 0, uid3, name, target->is_rom() ? 0 : (target->get_code_run_addr(attacher) - target->get_code_base()));

        kernel_system *kern = sys->get_kernel_system();

        if (kern->crr_process() == attacher)
            write_breakpoint_blocks(attacher);
    }

    void scripts::handle_uid_process_change(kernel::process *aff, const std::uint32_t old_one) {
        if (aff->get_uid() == old_one) {
            return;
        }

        kernel_system *kern = sys->get_kernel_system();

        for (auto &[addr, info] : breakpoints) {
            for (auto &list_hook : info.list_) {
                if (list_hook.attached_process_ != 0) {
                    if (list_hook.attached_process_ == old_one) {
                        list_hook.attached_process_ = aff->get_uid();
                    }
                }

                if (info.source_insts_.find(old_one) != info.source_insts_.end()) {
                    info.source_insts_[aff->get_uid()] = info.source_insts_[old_one];
                    info.source_insts_.erase(old_one);
                }
            }
        }
    }

    void scripts::handle_imb_range(kernel::process *p, const address addr, const std::size_t ss) {
        write_breakpoint_blocks(p);
    }
}
