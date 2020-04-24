/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <arm/arm_interface_extended.h>
#include <common/configure.h>

#include <manager/config.h>
#include <manager/manager.h>

#if ENABLE_SCRIPTING
#include <manager/script_manager.h>
#endif

#include <epoc/kernel.h>
#include <gdbstub/gdbstub.h>

namespace eka2l1::arm {
    arm_interface_extended::arm_interface_extended(gdbstub *stub, manager_system *mngr)
        : stub_(stub)
        , mngr_(mngr) {

    }
    
    void arm_interface_extended::handle_breakpoint(kernel_system *kern, manager::config_state *conf) {
        kernel::thread *crr_thread = kern->crr_thread();
        save_context(crr_thread->get_thread_context());

        kernel::process *pr = crr_thread->owning_process();

#if ENABLE_SCRIPTING
        // Call scripts first
        if (conf->enable_breakpoint_script) {
            stop();
    
            manager::script_manager *scripter = mngr_->get_script_manager();

            if (!last_breakpoint_script_hits_[crr_thread->unique_id()].hit_) {
                // Try to execute the real instruction, and put a breakpoint next to our instruction.
                // Later when we hit the next instruction, rewrite the bkpt
                const vaddress cur_addr = get_pc() | ((get_cpsr() & 0x20) >> 5);

                if (scripter->call_breakpoints(cur_addr, crr_thread->owning_process()->get_uid())) {
                    breakpoint_hit_info &info = last_breakpoint_script_hits_[crr_thread->unique_id()];
                    info.hit_ = true;
                    const std::uint32_t last_breakpoint_script_size_ = (get_cpsr() & 0x20) ? 2 : 4;
                    info.addr_ = cur_addr;

                    scripter->write_back_breakpoint(pr, cur_addr);
                    imb_range(cur_addr, 4);
                }
            }
        }
#endif

        if (stub_ && stub_->is_connected()) {
            stop();
            set_pc(get_pc());

            stub_->break_exec();
            stub_->send_trap_gdb(crr_thread, 5);
        }
    }

    bool arm_interface_extended::last_script_breakpoint_hit(kernel::thread *thr) {
        if (!thr) {
            return false;
        }

        return last_breakpoint_script_hits_[thr->unique_id()].hit_;
    }

    void arm_interface_extended::reset_breakpoint_hit(kernel_system *kern) {
        breakpoint_hit_info &info = last_breakpoint_script_hits_[kern->crr_thread()->unique_id()];
 
#if ENABLE_SCRIPTING
        manager::script_manager *scripter = mngr_->get_script_manager();
        scripter->write_breakpoint_block(kern->crr_process(), info.addr_);
#endif

        imb_range(info.addr_, 4);
        info.hit_ = false;
    }
}