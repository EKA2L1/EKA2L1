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

#include <manager/script_manager.h>

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include <scripting/instance.h>
#include <scripting/message.h>
#include <scripting/symemu.inl>
#include <scripting/thread.h>

#include <epoc/epoc.h>
#include <kernel/kernel.h>

namespace py = pybind11;

#ifndef _MSC_VER
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif

namespace eka2l1::manager {
    breakpoint_info::breakpoint_info()
        : addr_(0)
        , flags_(0)
        , attached_process_(0) {
    }

    script_manager::script_manager(system *sys)
        : sys(sys)
        , ipc_send_callback_handle(0)
        , ipc_complete_callback_handle(0)
        , thread_kill_callback_handle(0)
        , breakpoint_hit_callback_handle(0)
        , process_switch_callback_handle(0)
        , codeseg_loaded_callback_handle(0) {
        scripting::set_current_instance(sys);
    }

    script_manager::~script_manager() {
        kernel_system *kern = sys->get_kernel_system();

        if (ipc_send_callback_handle)
            kern->unregister_ipc_send_callback(ipc_send_callback_handle);
        
        if (ipc_complete_callback_handle)
            kern->unregister_ipc_complete_callback(ipc_complete_callback_handle);
        
        if (thread_kill_callback_handle)
            kern->unregister_thread_kill_callback(thread_kill_callback_handle);

        if (breakpoint_hit_callback_handle)
            kern->unregister_breakpoint_hit_callback(breakpoint_hit_callback_handle);

        if (process_switch_callback_handle)
            kern->unregister_process_switch_callback(process_switch_callback_handle);

        if (codeseg_loaded_callback_handle) {
            kern->get_lib_manager()->unregister_codeseg_loaded_callback(codeseg_loaded_callback_handle);
        }

        modules.clear();
        interpreter.release();
    }

    bool script_manager::import_module(const std::string &path) {
        const std::string name = eka2l1::filename(path);
        if (!interpreter) {
            interpreter = std::make_unique<pybind11::scoped_interpreter>();
        }

        if (modules.find(name) == modules.end()) {
            const auto &crr_path = fs::current_path();
            const auto &pr_path = fs::absolute(fs::path(path).parent_path());

            //std::lock_guard<std::mutex> guard(smutex);

            std::error_code sec;
            fs::current_path(pr_path, sec);

            try {
                modules.emplace(name.c_str(), py::module::import(name.data()));
            } catch (py::error_already_set &exec) {
                const char *description = exec.what();

                LOG_WARN("Script compile error: {}", description);
                fs::current_path(crr_path);

                return false;
            }

            fs::current_path(crr_path);

            if (!call_module_entry(name.c_str())) {
                // If the module entry failed, we still success, but not execute any futher method
                return true;
            }
        }

        return true;
    }

    bool script_manager::call_module_entry(const std::string &module) {
        if (!ipc_send_callback_handle) {
            kernel_system *kern = sys->get_kernel_system();
            
            ipc_send_callback_handle = kern->register_ipc_send_callback([this](const std::string& svr_name, const int ord, const ipc_arg& args, kernel::thread* callee) {
                call_ipc_send(svr_name, ord, args.args[0], args.args[1], args.args[2], args.args[3], args.flag, callee);
            });

            ipc_complete_callback_handle = kern->register_ipc_complete_callback([this](ipc_msg *msg, const std::int32_t complete_code) {
                call_ipc_complete(msg->msg_session->get_server()->name(), msg->function, msg);
            });

            thread_kill_callback_handle = kern->register_thread_kill_callback([this](kernel::thread* target, const std::string& cagetory, const std::int32_t reason) {
                call_panics(cagetory, reason);
            });

            breakpoint_hit_callback_handle = kern->register_breakpoint_hit_callback([this](arm::core *core, kernel::thread *correspond, const vaddress addr) {
                handle_breakpoint(core, correspond, addr);
            });

            process_switch_callback_handle = kern->register_process_switch_callback([this](arm::core *core, kernel::process *old_one, kernel::process *new_one) {
                handle_process_switch(core, old_one, new_one);
            });

            codeseg_loaded_callback_handle = kern->get_lib_manager()->register_codeseg_loaded_callback([this](const std::string& name, kernel::process *attacher, codeseg_ptr target) {
                handle_codeseg_loaded(name, attacher, target);
            });
        }

        if (modules.find(module) == modules.end()) {
            return false;
        }

        try {
            modules[module].attr("scriptEntry")();
        } catch (py::error_already_set &exec) {
            LOG_ERROR("Error executing script entry of {}: {}", module, exec.what());
            return false;
        }

        return true;
    }

    void script_manager::call_panics(const std::string &panic_cage, int err_code) {
        std::lock_guard<std::mutex> guard(smutex);

        eka2l1::system *crr_instance = scripting::get_current_instance();
        eka2l1::scripting::set_current_instance(sys);

        for (const auto &panic_function : panic_functions) {
            if (panic_function.first == panic_cage) {
                try {
                    panic_function.second(err_code);
                } catch (py::error_already_set &exec) {
                    LOG_WARN("Script interpreted error: {}", exec.what());
                }
            }
        }

        scripting::set_current_instance(crr_instance);
    }

    void script_manager::register_panic(const std::string &panic_cage, pybind11::function &func) {
        panic_functions.push_back(panic_func(panic_cage, func));
    }

    void script_manager::register_reschedule(pybind11::function &func) {
        reschedule_functions.push_back(func);
    }

    void script_manager::register_ipc(const std::string &server_name, const int opcode, const int invoke_when, pybind11::function &func) {
        ipc_functions[server_name][(static_cast<std::uint64_t>(opcode) | (static_cast<std::uint64_t>(invoke_when) << 32))].push_back(func);
    }

    void script_manager::write_breakpoint_block(kernel::process *pr, const vaddress target) {
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

    bool script_manager::write_back_breakpoint(kernel::process *pr, const vaddress target) {
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

    void script_manager::write_back_breakpoints(kernel::process *pr) {
        for (const auto &[addr, info] : breakpoints) {
            if (!info.list_.empty()) {
                write_back_breakpoint(pr, info.list_[0].addr_);
            }
        }
    }

    void script_manager::write_breakpoint_blocks(kernel::process *pr) {
        for (const auto &[addr, info] : breakpoints) {
            // The address on the info contains information about Thumb/ARM mode
            if (!info.list_.empty())
                write_breakpoint_block(pr, info.list_[0].addr_);
        }
    }

    void script_manager::register_library_hook(const std::string &name, const std::uint32_t ord, const std::uint32_t process_uid, pybind11::function &func) {
        const std::string lib_name_lower = common::lowercase_string(name);

        breakpoint_info info;
        info.lib_name_ = lib_name_lower;
        info.flags_ = breakpoint_info::FLAG_IS_ORDINAL;
        info.addr_ = ord;
        info.invoke_ = func;
        info.attached_process_ = process_uid;

        breakpoint_wait_patch.push_back(info);
    }

    void script_manager::register_breakpoint(const std::string &lib_name, const uint32_t addr, const std::uint32_t process_uid, pybind11::function &func) {
        const std::string lib_name_lower = common::lowercase_string(lib_name);

        breakpoint_info info;
        info.lib_name_ = lib_name_lower;
        info.flags_ = breakpoint_info::FLAG_BASED_IMAGE;
        info.addr_ = addr;
        info.invoke_ = func;
        info.attached_process_ = process_uid;

        breakpoint_wait_patch.push_back(info);
    }

    void script_manager::patch_library_hook(const std::string &name, const std::vector<vaddress> &exports) {
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

    void script_manager::patch_unrelocated_hook(const std::uint32_t process_uid, const std::string &name, const address new_code_addr) {
        const std::string lib_name_lower = common::lowercase_string(name);

        for (breakpoint_info &breakpoint: breakpoint_wait_patch) {
            if (((breakpoint.attached_process_ == 0) || (breakpoint.attached_process_ == process_uid)) && (breakpoint.lib_name_ == lib_name_lower)
                && (breakpoint.flags_ & breakpoint_info::FLAG_BASED_IMAGE)) {
                breakpoint.addr_ += new_code_addr;
                breakpoint.flags_ &= ~breakpoint_info::FLAG_BASED_IMAGE;

                breakpoints[breakpoint.addr_ & ~1].list_.push_back(breakpoint);
            }
        }
    }

    void script_manager::call_ipc_send(const std::string &server_name, const int opcode, const std::uint32_t arg0, const std::uint32_t arg1,
        const std::uint32_t arg2, const std::uint32_t arg3, const std::uint32_t flags,
        kernel::thread *callee) {
        std::lock_guard<std::mutex> guard(smutex);

        eka2l1::system *crr_instance = scripting::get_current_instance();
        eka2l1::scripting::set_current_instance(sys);

        for (const auto &ipc_func : ipc_functions[server_name][opcode]) {
            try {
                ipc_func(arg0, arg1, arg2, arg3, flags, std::make_unique<scripting::thread>(reinterpret_cast<std::uint64_t>(callee)));
            } catch (py::error_already_set &exec) {
                LOG_WARN("Script interpreted error: {}", exec.what());
            }
        }

        scripting::set_current_instance(crr_instance);
    }

    void script_manager::call_ipc_complete(const std::string &server_name,
        const int opcode, ipc_msg *msg) {
        std::lock_guard<std::mutex> guard(smutex);

        eka2l1::system *crr_instance = scripting::get_current_instance();
        eka2l1::scripting::set_current_instance(sys);

        for (const auto &ipc_func : ipc_functions[server_name][(2ULL << 32) | opcode]) {
            try {
                ipc_func(std::make_unique<scripting::ipc_message_wrapper>(
                    reinterpret_cast<std::uint64_t>(msg)));
            } catch (py::error_already_set &exec) {
                LOG_WARN("Script interpreted error: {}", exec.what());
            }
        }

        scripting::set_current_instance(crr_instance);
    }

    void script_manager::call_reschedules() {
        std::lock_guard<std::mutex> guard(smutex);

        eka2l1::system *crr_instance = scripting::get_current_instance();
        eka2l1::scripting::set_current_instance(sys);

        for (const auto &reschedule_func : reschedule_functions) {
            try {
                reschedule_func();
            } catch (py::error_already_set &exec) {
                LOG_WARN("Script interpreted error: {}", exec.what());
            }
        }

        scripting::set_current_instance(crr_instance);
    }

    bool script_manager::call_breakpoints(const std::uint32_t addr, const std::uint32_t process_uid) {
        if (breakpoints.find(addr & ~1) == breakpoints.end()) {
            return false;
        }

        breakpoint_info_list &list = breakpoints[addr & ~1].list_;

        for (const auto &info : list) {
            if ((info.attached_process_ != 0) && (info.attached_process_ != process_uid)) {
                continue;
            }

            try {
                info.invoke_();
            } catch (py::error_already_set &exec) {
                LOG_WARN("Script interpreted error: {}", exec.what());
            }
        }

        return true;
    }
    
    bool script_manager::last_breakpoint_hit(kernel::thread *thr) {
        if (!thr) {
            return false;
        }

        return last_breakpoint_script_hits[thr->unique_id()].hit_;
    }

    void script_manager::reset_breakpoint_hit(arm::core *running_core, kernel::thread *thr) {
        breakpoint_hit_info &info = last_breakpoint_script_hits[thr->unique_id()];
        write_breakpoint_block(thr->owning_process(), info.addr_);

        running_core->imb_range((info.addr_ & ~1), (info.addr_ & 1) ? 2 : 4);
        info.hit_ = false;
    }

    void script_manager::handle_breakpoint(arm::core *running_core, kernel::thread *correspond, const std::uint32_t addr) {
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
                running_core->imb_range(addr, last_breakpoint_script_size_);
            }
        }
    }

    void script_manager::handle_process_switch(arm::core *core_switch, kernel::process *old_friend, kernel::process *new_friend) {
        if (old_friend) {
            write_back_breakpoints(old_friend);
        }

        write_breakpoint_blocks(new_friend);
    }

    void script_manager::handle_codeseg_loaded(const std::string &name, kernel::process *attacher, codeseg_ptr target) {
        patch_library_hook(name, target->get_export_table_raw());
        patch_unrelocated_hook(attacher ? (attacher->get_uid()) : 0, name, target->is_rom() ? 0 : (target->get_code_run_addr(attacher) - target->get_code_base()));
    }
}
