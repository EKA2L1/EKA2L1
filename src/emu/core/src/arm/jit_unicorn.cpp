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

#include <common/algorithm.h>
#include <common/log.h>
#include <common/types.h>

#include <core/arm/jit_unicorn.h>
#include <core/core.h>
#include <core/core_timing.h>
#include <core/disasm/disasm.h>
#include <core/gdbstub/gdbstub.h>
#include <core/hle/libmanager.h>
#include <core/manager/manager.h>
#include <core/ptr.h>

#include <unicorn/unicorn.h>

#include <cassert>

bool noppify_func = false;

bool thumb_mode(uc_engine *uc) {
    size_t mode = 0;
    auto err = uc_query(uc, UC_QUERY_MODE, &mode);

    if (err != UC_ERR_OK) {
        LOG_INFO("Query mode failed");
        return false;
    }

    return mode & UC_MODE_THUMB;
}

void dump_context(eka2l1::arm::jit_interface::thread_context uni) {
    LOG_TRACE("Dumping CPU context: ");
    LOG_TRACE("pc: 0x{:x}", uni.pc);
    LOG_TRACE("lr: 0x{:x}", uni.lr);
    LOG_TRACE("sp: 0x{:x}", uni.sp);
    LOG_TRACE("cpsr: 0x{:x}", uni.cpsr);

    for (size_t i = 0; i < uni.cpu_registers.size(); i++) {
        LOG_TRACE("r{}: 0x{:x}", i, uni.cpu_registers[i]);
    }
}

void read_hook(uc_engine *uc, uc_mem_type type, uint32_t address, int size, int64_t value, void *user_data) {
    eka2l1::arm::jit_unicorn *jit = reinterpret_cast<decltype(jit)>(user_data);

    if (jit == nullptr) {
        LOG_ERROR("Read hook failed: User Data was null");
        return;
    }

    eka2l1::memory_system *mem = jit->get_memory_sys();
    mem->read(address, &value, size);

    // It's hacky, and im not ok with this
    bool read_log = jit->get_lib_manager()->get_sys()->get_bool_config("log_read");

    if (read_log)
        LOG_TRACE("Read at address = 0x{:x}, size = 0x{:x}, val = 0x{:x}", address, size, value);
}

void write_hook(uc_engine *uc, uc_mem_type type, uint32_t address, int size, int64_t value, void *user_data) {
    eka2l1::arm::jit_unicorn *jit = reinterpret_cast<decltype(jit)>(user_data);

    if (jit == nullptr) {
        LOG_ERROR("Read hook failed: User Data was null");
        return;
    }

    bool write_log = jit->get_lib_manager()->get_sys()->get_bool_config("log_write");

    eka2l1::memory_system *mem = jit->get_memory_sys();

    if (write_log)
        LOG_TRACE("Write at address = 0x{:x}, size = 0x{:x}, val = 0x{:x}", address, size, value);
}

void code_hook(uc_engine *uc, uint32_t address, uint32_t size, void *user_data) {
    eka2l1::arm::jit_unicorn *jit = reinterpret_cast<decltype(jit)>(user_data);
    eka2l1::arm::jit_interface::thread_context context_debug;

    if (jit == nullptr) {
        LOG_ERROR("Code hook failed: User Data was null");
        return;
    }

    jit->save_context(context_debug);

    bool enable_breakpoint_script = 
        jit->get_lib_manager()->get_sys()->get_bool_config("enable_breakpoint_script");

    eka2l1::hle::lib_manager *mngr = jit->get_lib_manager();

    bool log_code = jit->get_lib_manager()->get_sys()->get_bool_config("log_code");
    bool log_passed = jit->get_lib_manager()->get_sys()->get_bool_config("log_passed");

    if (enable_breakpoint_script) {
        jit->get_manager_sys()->get_script_manager()->call_breakpoints(address);
        jit->get_manager_sys()->get_script_manager()->call_breakpoints(address + 1);
    }

    if (log_passed && mngr) {
        auto res = mngr->get_sid(address);

        if (!res && thumb_mode(uc)) {
            res = mngr->get_sid(address + 1);
        }

        if (res) {
            LOG_INFO("Passing through: {} addr = 0x{:x}", *mngr->get_func_name(*res), address);
        }
    }

    if (log_code) {        
        const uint8_t *code = eka2l1::ptr<const uint8_t>(address).get(jit->get_memory_sys());
        size_t buffer_size = eka2l1::common::GB(4) - address;
        bool thumb = thumb_mode(uc);
        std::string disassembly = jit->get_disasm_sys()->disassemble(code, buffer_size, address, thumb);

        LOG_TRACE("{:#08x} {} 0x{:x}", address, disassembly, thumb ? *(uint16_t *)code : *(uint32_t *)code);
    }

    eka2l1::gdbstub *stub = jit->get_gdb_stub();

    if (stub) {
        eka2l1::breakpoint_address bkpt = stub->get_next_breakpoint_from_addr(
            address, eka2l1::breakpoint_type::Execute);

        if (stub->is_memory_break() && bkpt.type != eka2l1::breakpoint_type::None
            && address == bkpt.address) {
            jit->record_break(bkpt);
            uc_emu_stop(uc);
        }
    }
}

// Read the symbol and redirect to HLE function
void intr_hook(uc_engine *uc, uint32_t int_no, void *user_data) {
    eka2l1::arm::jit_unicorn *jit = reinterpret_cast<decltype(jit)>(user_data);

    if (jit == nullptr) {
        LOG_ERROR("Code hook failed: User Data was null");
        return;
    }

    uint32_t imm = 0;

    bool thumb = thumb_mode(uc);

    if (thumb) {
        uint16_t svc_inst = 0;

        address svca = jit->get_pc() - 2;
        uc_mem_read(uc, svca, &svc_inst, 2);
        imm = svc_inst & 0xff;
    } else {
        uint32_t svc_inst = 0;

        address svca = jit->get_pc() - 4;
        uc_mem_read(uc, svca, &svc_inst, 4);
        imm = svc_inst & 0xffffff;
    }

    if (imm == 0x00900000) {
        uint32_t sid = *eka2l1::ptr<uint32_t>(jit->get_pc() + 4).get(jit->get_memory_sys());
        auto func_name = jit->get_lib_manager()->get_func_name(sid);

        if (func_name) {
            LOG_INFO("Calling {} [0x{:x}]", *func_name, jit->get_pc() + 4);
        }

        jit->get_lib_manager()->call_hle(sid);
        return;
    } else if (imm == 0x00900001) {
        jit->get_lib_manager()->call_custom_hle(*eka2l1::ptr<uint32_t>(jit->get_pc() + 4).get(jit->get_memory_sys()));
        return;
    } else if (imm == 0x00900002) {
        uint32_t call_addr = jit->get_pc();

        if (call_addr && thumb) {
            call_addr -= 1;
        } else {
            call_addr -= 4;
        }

        bool res = jit->get_lib_manager()->call_hle(
            *jit->get_lib_manager()->get_sid(call_addr));

        if (res) {
            uint32_t lr = 0;
            uc_reg_read(uc, UC_ARM_REG_LR, &lr);

            jit->set_pc(lr);
        }

        return;
    }

    if (!jit->get_lib_manager()->call_svc(imm)) {
        LOG_WARN("Unimplement SVC call: 0x{:x}", imm);
    }
}

namespace eka2l1 {
    namespace arm {
        void enable_vfp_fp(uc_engine *en) {
            uint64_t c1c0 = 0;
            uc_err err = uc_reg_read(en, UC_ARM_REG_C1_C0_2, &c1c0);

            if (err != UC_ERR_OK) {
                LOG_WARN("Can't get c1c0, set vfp failed!");
                return;
            }

            c1c0 |= 0xF << 20;
            err = uc_reg_write(en, UC_ARM_REG_C1_C0_2, &c1c0);

            if (err != UC_ERR_OK) {
                LOG_WARN("Can't set c1c0, set vfp failed!");
                return;
            }
            // Enable fpu
            const uint64_t fpexc = 0xF0000000;

            err = uc_reg_write(en, UC_ARM_REG_FPEXC, &fpexc);

            if (err != UC_ERR_OK) {
                LOG_WARN("Can't set fpexc, set floating point precison failed!");
                return;
            }
        }

        bool jit_unicorn::is_thumb_mode() {
            return thumb_mode(engine);
        }

        jit_unicorn::jit_unicorn(kernel_system *kern, timing_system *sys, manager_system *mngr, 
            memory_system *mem, disasm *asmdis, hle::lib_manager *lmngr, gdbstub *stub)
            : timing(sys)
            , mem(mem)
            , asmdis(asmdis)
            , lib_mngr(lmngr)
            , stub(stub)
            , kern(kern)
            , mngr(mngr) {
            uc_err err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &engine);
            assert(err == UC_ERR_OK);

            uc_hook hook{};

            uc_hook_add(engine, &hook, UC_HOOK_MEM_READ, reinterpret_cast<void *>(read_hook), this, 1, 0);
            uc_hook_add(engine, &hook, UC_HOOK_MEM_WRITE, reinterpret_cast<void *>(write_hook), this, 1, 0);
            uc_hook_add(engine, &hook, UC_HOOK_CODE, reinterpret_cast<void *>(code_hook), this, 1, 0);
            uc_hook_add(engine, &hook, UC_HOOK_INTR, reinterpret_cast<void *>(intr_hook), this, 1, 0);
            assert(err == UC_ERR_OK);

            enable_vfp_fp(engine);

            if (stub && stub->is_server_enabled()) {
                last_breakpoint_hit = false;
            }
        }

        jit_unicorn::~jit_unicorn() {
            uc_close(engine);
        }

        bool jit_unicorn::execute_instructions(uint32_t num_instructions) {
            uint32_t pc = get_pc();
            uc_err err;

            if (num_instructions >= 1 || num_instructions == -1) {
                err = uc_emu_start(engine, pc, 1ULL << 63, 0, num_instructions == -1 ? 0 : num_instructions);

                if (err != UC_ERR_OK) {
                    uint32_t error_pc = get_pc();
                    uint32_t lr = 0;
                    uc_reg_read(engine, UC_ARM_REG_LR, &lr);

                    LOG_CRITICAL("Unicorn error {} at: start PC: {:#08x} error PC {:#08x} LR: {:#08x}",
                        uc_strerror(err), pc, error_pc, lr);

                    thread_context context;

                    save_context(context);
                    dump_context(context);

                    system *sys = get_lib_manager()->get_sys();
                    thread_ptr thr = sys->get_kernel_system()->crr_thread();

                    thr->stop();

                    return false;
                }

                assert(err == UC_ERR_OK);

                if (timing) {
                    timing->add_ticks(num_instructions - 1);
                }

                if (stub && stub->is_server_enabled()) {
                    if (last_breakpoint_hit) {
                        set_pc(last_breakpoint.address);
                    }

                    thread_ptr crr_thread = kern->crr_thread();
                    save_context(crr_thread->get_thread_context());

                    if (last_breakpoint_hit && stub->get_cpu_step_flag()) {
                        last_breakpoint_hit = false;
                        
                        stub->break_exec();
                        stub->send_trap_gdb(crr_thread, 5);
                    }
                }
            }

            return true;
        }

        void jit_unicorn::run() {
            if (!timing) {
                execute_instructions(-1);
                return;
            }

            execute_instructions(std::max(timing->get_downcount(), 0));
        }

        void jit_unicorn::stop() {
            uc_err force_stop = uc_emu_stop(engine);

            if (force_stop != UC_ERR_OK) {
                LOG_ERROR("Can't stop CPU, reason: {}", force_stop);
            }
        }

        void jit_unicorn::step() {
            execute_instructions(1);
        }

        uint32_t jit_unicorn::get_reg(size_t idx) {
            uint32_t val = 0;
            auto treg = UC_ARM_REG_R0 + static_cast<uint8_t>(idx);
            auto err = uc_reg_read(engine, treg, &val);

            if (err != UC_ERR_OK) {
                LOG_ERROR("Failed to get ARM CPU registers.");
            }

            return val;
        }

        uint32_t jit_unicorn::get_sp() {
            uint32_t ret = 0;
            auto err = uc_reg_read(engine, UC_ARM_REG_SP, &ret);

            if (err != UC_ERR_OK) {
                LOG_WARN("Failed to read ARM CPU SP!");
            }

            return ret;
        }

        uint32_t jit_unicorn::get_pc() {
            uint32_t ret = 0;
            auto err = uc_reg_read(engine, UC_ARM_REG_PC, &ret);

            if (err != UC_ERR_OK) {
                LOG_WARN("Failed to read ARM CPU PC!");
            }

            return ret;
        }

        uint32_t jit_unicorn::get_vfp(size_t idx) {
            return 0;
        }

        void jit_unicorn::set_reg(size_t idx, uint32_t val) {
            auto treg = UC_ARM_REG_R0 + static_cast<uint8_t>(idx);
            auto err = uc_reg_write(engine, treg, &val);

            if (err != UC_ERR_OK) {
                LOG_ERROR("Failed to set ARM CPU registers.");
            }
        }

        void jit_unicorn::set_pc(uint32_t pc) {
            auto err = uc_reg_write(engine, UC_ARM_REG_PC, &pc);

            if (err != UC_ERR_OK) {
                LOG_WARN("ARM PC failed to be set!");
            }
        }

        void jit_unicorn::set_entry_point(address ep) {
            LOG_TRACE("Entry point set: 0x{:x}", ep);
            epa = ep;
            set_pc(epa);
        }

        address jit_unicorn::get_entry_point() {
            return epa;
        }

        void jit_unicorn::set_lr(uint32_t val) {
            auto err = uc_reg_write(engine, UC_ARM_REG_LR, &val);

            if (err != UC_ERR_OK) {
                LOG_WARN("ARM PC failed to be set!");
            }
        }

        void jit_unicorn::set_stack_top(address addr) {
            auto err = uc_reg_write(engine, UC_ARM_REG_SP, &addr);

            if (err != UC_ERR_OK) {
                LOG_WARN("ARM SP failed to be set!");
            }
        }

        address jit_unicorn::get_stack_top() {
            address addr = 0;
            auto err = uc_reg_read(engine, UC_ARM_REG_SP, &addr);

            if (err != UC_ERR_OK) {
                LOG_WARN("Failed to get ARM CPU stack top!");
            }

            return addr;
        }

        void jit_unicorn::set_vfp(size_t idx, uint32_t val) {
        }

        uint32_t jit_unicorn::get_cpsr() {
            uint32_t cpsr = 0;
            auto err = uc_reg_read(engine, UC_ARM_REG_CPSR, &cpsr);

            return cpsr;
        }

        uint32_t jit_unicorn::get_lr() {
            uint32_t lr = 0;
            auto err = uc_reg_read(engine, UC_ARM_REG_LR, &lr);

            return lr;
        }

        void jit_unicorn::set_cpsr(uint32_t val) {
            auto err = uc_reg_write(engine, UC_ARM_REG_CPSR, &val);

            if (err != UC_ERR_OK) {
                LOG_ERROR("Writing cpsr failed!");
            }
        }

        void jit_unicorn::set_sp(uint32_t val) {
            set_stack_top(val);
        }

        void jit_unicorn::save_context(thread_context &ctx) {
            for (auto i = 0; i < ctx.cpu_registers.size(); i++) {
                ctx.cpu_registers[i] = get_reg(i);
            }

            for (auto i = 0; i < ctx.fpu_registers.size(); i++) {
                uc_err err = uc_reg_read(engine, UC_ARM_REG_D0, &(ctx.fpu_registers[i]));
            }

            uc_reg_read(engine, UC_ARM_REG_FPSCR, &(ctx.fpscr));

            ctx.sp = get_sp();
            ctx.lr = get_lr();
            ctx.pc = get_pc();
            ctx.cpsr = get_cpsr();
        }

        void jit_unicorn::load_context(const thread_context &ctx) {
            for (auto i = 0; i < ctx.cpu_registers.size(); i++) {
                set_reg(i, ctx.cpu_registers[i]);
            }

            for (auto i = 0; i < ctx.fpu_registers.size(); i++) {
                uc_err err = uc_reg_write(engine, UC_ARM_REG_D0, &(ctx.fpu_registers[i]));
            }

            uc_reg_write(engine, UC_ARM_REG_FPSCR, &(ctx.fpscr));

            set_sp(ctx.sp);
            set_lr(ctx.lr);
            set_pc(ctx.pc);
            set_cpsr(ctx.cpsr);
        }

        void jit_unicorn::prepare_rescheduling() {
            uc_err err = uc_emu_stop(engine);

            if (err != UC_ERR_OK) {
                LOG_ERROR("Prepare rescheduling failed!");
            }
        }

        void jit_unicorn::page_table_changed() {
        }

        void jit_unicorn::map_backing_mem(address vaddr, size_t size, uint8_t *ptr, prot protection) {
            uint32_t perms = 0;

            switch (protection) {
            case prot::read:
                perms = UC_PROT_READ;
                break;

            case prot::read_exec:
                perms = UC_PROT_READ | UC_PROT_EXEC;
                break;

            case prot::read_write:
                perms = UC_PROT_READ | UC_PROT_WRITE;
                break;

            case prot::write:
                perms = UC_PROT_WRITE;
                break;

            case prot::exec:
                perms = UC_PROT_EXEC;
                break;

            case prot::read_write_exec:
                perms = UC_PROT_READ | UC_PROT_WRITE | UC_PROT_EXEC;
                break;

            default:
                break;
            }

            uc_err err = uc_mem_map_ptr(engine, vaddr, size, perms, ptr);

            if (err != UC_ERR_OK) {
                LOG_WARN("Error mapping backing memory at addr: 0x{:x}, err: {}", vaddr, uc_strerror(err));
            }
        }

        void jit_unicorn::unmap_memory(address addr, size_t size) {
            uc_err err = uc_mem_unmap(engine, addr, size);

            if (addr >= 0x80000000) {
                LOG_WARN("Trying to unmap ROM");
            }

            if (err != UC_ERR_OK) {
                LOG_WARN("Error unmapping backing memory at addr: 0x{:x}, err: {}", addr, uc_strerror(err));
            }
        }

        void jit_unicorn::clear_instruction_cache() {
            // Empty
        }
    }
}
