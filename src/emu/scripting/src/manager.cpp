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
#include <common/log.h>
#include <common/path.h>
#include <common/platform.h>

#ifndef _MSC_VER
#include <experimental/filesystem>
#else
#include <filesystem>
#endif

#include <scripting/manager.h>

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include <scripting/instance.h>
#include <scripting/message.h>
#include <scripting/symemu.inl>
#include <scripting/thread.h>

#include <system/epoc.h>
#include <kernel/kernel.h>

namespace py = pybind11;

#ifndef _MSC_VER
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

// explicit deduction guide (not needed as of C++20)
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace eka2l1::manager {
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
        , codeseg_loaded_callback_handle(0) {
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

        modules.clear();
        interpreter.release();
    }

    bool scripts::import_module(const std::string &path) {
        const std::string name_full = eka2l1::filename(path);
        const std::string name = eka2l1::replace_extension(name_full, "");

        if (!interpreter) {
            interpreter = std::make_unique<pybind11::scoped_interpreter>();
        }

        if (modules.find(name) == modules.end()) {
            const auto &crr_path = fs::current_path();
            const auto &pr_path = fs::absolute(fs::path(path).parent_path());

            std::lock_guard<std::mutex> guard(smutex);

            std::error_code sec;
            fs::current_path(pr_path, sec);

            if (eka2l1::path_extension(path) == ".lua") {
                lua_State *new_state = luaL_newstate();
                luaL_openlibs(new_state);

                if (luaL_loadfile(new_state, name_full.c_str())  == LUA_OK) {
                    modules.emplace(name.c_str(), scripting::luacpp_state(new_state));
                } else {
                    LOG_WARN(SCRIPTING, "Fail to load script {}, error {}", name, lua_tostring(new_state, -1));
                    lua_close(new_state);
                }
            } else {
                try {
                    modules.emplace(name.c_str(), py::module::import(name.data()));
                } catch (py::error_already_set &exec) {
                    const char *description = exec.what();

                    LOG_WARN(SCRIPTING, "Script compile error: {}", description);
                    fs::current_path(crr_path);

                    return false;
                }
            }

            if (!call_module_entry(name.c_str())) {
                // If the module entry failed, we still success, but not execute any futher method            
                fs::current_path(crr_path);
                return true;
            }
            
            fs::current_path(crr_path);
        }

        return true;
    }

    bool scripts::call_module_entry(const std::string &module) {
        if (!ipc_send_callback_handle) {
            kernel_system *kern = sys->get_kernel_system();
            
            ipc_send_callback_handle = kern->register_ipc_send_callback([this](const std::string& svr_name, const int ord, const ipc_arg& args, kernel::thread* callee) {
                call_ipc_send(svr_name, ord, args.args[0], args.args[1], args.args[2], args.args[3], args.flag, callee);
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

            codeseg_loaded_callback_handle = kern->register_codeseg_loaded_callback([this](const std::string& name, kernel::process *attacher, codeseg_ptr target) {
                handle_codeseg_loaded(name, attacher, target);
            });

            uid_change_callback_handle = kern->register_uid_process_change_callback([this](kernel::process *aff, kernel::process_uid_type type) {
                handle_uid_process_change(aff, std::get<2>(type));
            });
        }

        if (modules.find(module) == modules.end()) {
            return false;
        }

        bool fine = true;

        std::visit(overloaded {
            [&](pybind11::module &modobj) {
                try {
                    modobj.attr("scriptEntry")(); 
                } catch (py::error_already_set &exec) {
                    LOG_ERROR(SCRIPTING, "Error executing script entry of {}: {}", module, exec.what());
                    fine = false;
                }
            },
            [&](scripting::luacpp_state &state) {
                if (lua_pcall(state.state_, 0, 1, 0) == LUA_OK) {
                    lua_pop(state.state_, lua_gettop(state.state_));
                } else {
                    LOG_ERROR(SCRIPTING, "Error executing script entry of {}: {}", module, lua_tostring(state.state_, -1));
                    fine = false;
                }
            }
        }, modules[module]);

        return fine;
    }

    void scripts::register_ipc(const std::string &server_name, const int opcode, const int invoke_when, pybind11::function &func) {
        ipc_functions[server_name][(static_cast<std::uint64_t>(opcode) | (static_cast<std::uint64_t>(invoke_when) << 32))].push_back(func);
    }

    void scripts::write_breakpoint_block(kernel::process *pr, const vaddress target) {
        const vaddress aligned = target & ~1;

        std::uint32_t *data = reinterpret_cast<std::uint32_t *>(pr->get_ptr_on_addr_space(aligned));
        auto ite = std::find_if(breakpoints[aligned].list_.begin(), breakpoints[aligned].list_.end(),
            [=](const breakpoint_info &info) { return (info.attached_process_ == 0) || (info.attached_process_ == pr->get_uid()); });

        auto &source_insts = breakpoints[aligned].source_insts_;

        if (ite == breakpoints[aligned].list_.end() || source_insts.find(pr->get_uid()) != source_insts.end()) {
            return;
        }

        source_insts[pr->get_uid()] = data[0];

        if (target & 1) {
            // The target destination is thumb
            *reinterpret_cast<std::uint16_t *>(data) = 0xBE00;
        } else {
            data[0] = 0xE1200070; // bkpt #0
        }
    }

    bool scripts::write_back_breakpoint(kernel::process *pr, const vaddress target) {
        auto &sources = breakpoints[target & ~1].source_insts_;
        auto source_value = sources.find(pr->get_uid());

        if (source_value == sources.end()) {
            return false;
        }

        std::uint32_t *data = reinterpret_cast<std::uint32_t *>(pr->get_ptr_on_addr_space(target & ~1));
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
        for (const auto &[addr, info] : breakpoints) {
            // The address on the info contains information about Thumb/ARM mode
            if (!info.list_.empty())
                write_breakpoint_block(pr, info.list_[0].addr_);
        }
    }

    void scripts::register_library_hook(const std::string &name, const std::uint32_t ord, const std::uint32_t process_uid, breakpoint_hit_func func) {
        const std::string lib_name_lower = common::lowercase_string(name);

        breakpoint_info info;
        info.lib_name_ = lib_name_lower;
        info.flags_ = breakpoint_info::FLAG_IS_ORDINAL;
        info.addr_ = ord;
        info.invoke_ = func;
        info.attached_process_ = process_uid;

        breakpoint_wait_patch.push_back(info);
    }

    void scripts::register_breakpoint(const std::string &lib_name, const uint32_t addr, const std::uint32_t process_uid, breakpoint_hit_func func) {
        const std::string lib_name_lower = common::lowercase_string(lib_name);

        breakpoint_info info;
        info.lib_name_ = lib_name_lower;
        info.flags_ = breakpoint_info::FLAG_BASED_IMAGE;
        info.addr_ = addr;
        info.invoke_ = func;
        info.attached_process_ = process_uid;

        breakpoint_wait_patch.push_back(info);
    }

    void scripts::patch_library_hook(const std::string &name, const std::vector<vaddress> &exports) {
        const std::string lib_name_lower = common::lowercase_string(name);

        for (auto &breakpoint : breakpoint_wait_patch) {
            if ((breakpoint.flags_ & breakpoint_info::FLAG_IS_ORDINAL) && (breakpoint.lib_name_ == lib_name_lower)) {
                breakpoint.addr_ = exports[breakpoint.addr_ - 1];

                // It's now based on image. Only need rebase
                breakpoint.flags_ &= ~breakpoint_info::FLAG_IS_ORDINAL;
                breakpoint.flags_ |= breakpoint_info::FLAG_BASED_IMAGE;
            }
        }
    }

    void scripts::patch_unrelocated_hook(const std::uint32_t process_uid, const std::string &name, const address new_code_addr) {
        const std::string lib_name_lower = common::lowercase_string(name);
        for (breakpoint_info &breakpoint: breakpoint_wait_patch) {
            if (((breakpoint.attached_process_ == 0) || (breakpoint.attached_process_ == process_uid)) && (breakpoint.lib_name_ == lib_name_lower)
                && (breakpoint.flags_ & breakpoint_info::FLAG_BASED_IMAGE)) {
                breakpoint_info patched = breakpoint;

                patched.addr_ += new_code_addr;
                patched.flags_ &= ~breakpoint_info::FLAG_BASED_IMAGE;

                breakpoints[patched.addr_ & ~1].list_.push_back(patched);
            }
        }
    }

    void scripts::call_ipc_send(const std::string &server_name, const int opcode, const std::uint32_t arg0, const std::uint32_t arg1,
        const std::uint32_t arg2, const std::uint32_t arg3, const std::uint32_t flags,
        kernel::thread *callee) {
        std::lock_guard<std::mutex> guard(smutex);

        eka2l1::system *crr_instance = scripting::get_current_instance();
        eka2l1::scripting::set_current_instance(sys);

        for (const auto &ipc_func : ipc_functions[server_name][opcode]) {
            try {
                ipc_func(arg0, arg1, arg2, arg3, flags, std::make_unique<scripting::thread>(reinterpret_cast<std::uint64_t>(callee)));
            } catch (py::error_already_set &exec) {
                LOG_WARN(SCRIPTING, "Script interpreted error: {}", exec.what());
            }
        }

        scripting::set_current_instance(crr_instance);
    }

    void scripts::call_ipc_complete(const std::string &server_name,
        const int opcode, ipc_msg *msg) {
        std::lock_guard<std::mutex> guard(smutex);

        eka2l1::system *crr_instance = scripting::get_current_instance();
        eka2l1::scripting::set_current_instance(sys);

        for (const auto &ipc_func : ipc_functions[server_name][(2ULL << 32) | opcode]) {
            try {
                ipc_func(std::make_unique<scripting::ipc_message_wrapper>(
                    reinterpret_cast<std::uint64_t>(msg)));
            } catch (py::error_already_set &exec) {
                LOG_WARN(SCRIPTING, "Script interpreted error: {}", exec.what());
            }
        }

        scripting::set_current_instance(crr_instance);
    }

    bool scripts::call_breakpoints(const std::uint32_t addr, const std::uint32_t process_uid) {
        if (breakpoints.find(addr & ~1) == breakpoints.end()) {
            return false;
        }

        breakpoint_info_list &list = breakpoints[addr & ~1].list_;

        for (auto &info : list) {
            if ((info.attached_process_ != 0) && (info.attached_process_ != process_uid)) {
                continue;
            }

            std::visit(overloaded {
                [](pybind11::function &func) {
                    try {
                        func(); 
                    } catch (py::error_already_set &exec) {    
                        LOG_WARN(SCRIPTING, "Script interpreted error: {}", exec.what());
                    }
                },
                [](breakpoint_hit_lua_func func) {
                    func();
                }
            }, info.invoke_);
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
        breakpoint_hit_info &info = last_breakpoint_script_hits[thr->unique_id()];
        write_breakpoint_block(thr->owning_process(), info.addr_);

        running_core->imb_range((info.addr_ & ~1), (info.addr_ & 1) ? 2 : 4);
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
        patch_library_hook(name, target->get_export_table_raw());
        patch_unrelocated_hook(attacher ? (attacher->get_uid()) : 0, name, target->is_rom() ? 0 : (target->get_code_run_addr(attacher) - target->get_code_base()));

        kernel_system *kern = sys->get_kernel_system();

        if (kern->crr_process() == attacher)
            write_breakpoint_blocks(attacher);
    }

    void scripts::handle_uid_process_change(kernel::process *aff, const std::uint32_t old_one) {
        kernel_system *kern = sys->get_kernel_system();
        
        for (auto &[addr, info] : breakpoints) {
            for (auto &list_hook: info.list_) {
                if (list_hook.attached_process_ == old_one) {
                    list_hook.attached_process_ = aff->get_uid();

                    if (kern->crr_process() == aff) {
                        write_back_breakpoint(aff, info.list_[0].addr_);
                    }
                }

                if (info.source_insts_.find(old_one) != info.source_insts_.end()) {
                    info.source_insts_[aff->get_uid()] = info.source_insts_[old_one];
                    info.source_insts_.erase(old_one);
                }
            }
        }
    }
}
