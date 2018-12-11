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

#include <arm/arm_dynarmic.h>
#include <arm/arm_utils.h>
#include <disasm/disasm.h>
#include <epoc/kernel.h>
#include <epoc/mem.h>
#include <epoc/timing.h>
#include <epoc/kernel/libmanager.h>

#include <gdbstub/gdbstub.h>

#include <debugger/debugger.h>
#include <dynarmic/A32/context.h>
#include <dynarmic/A32/coprocessor.h>

namespace eka2l1 {
    namespace arm {
        class arm_dynarmic_cp15 : public Dynarmic::A32::Coprocessor {
        public:
            using coproc_reg = Dynarmic::A32::CoprocReg;

            explicit arm_dynarmic_cp15() {

            }

            ~arm_dynarmic_cp15() override {

            }

            std::optional<Callback> CompileInternalOperation(bool two, unsigned opc1, coproc_reg CRd,
                coproc_reg CRn, coproc_reg CRm,
                unsigned opc2) override {
                return std::nullopt;
            }

            CallbackOrAccessOneWord CompileSendOneWord(bool two, unsigned opc1, coproc_reg CRn,
                coproc_reg CRm, unsigned opc2) override {
                return boost::blank{};
            }

            CallbackOrAccessTwoWords CompileSendTwoWords(bool two, unsigned opc, coproc_reg CRm) override {
                return boost::blank{};
            }

            CallbackOrAccessOneWord CompileGetOneWord(bool two, unsigned opc1, coproc_reg CRn, coproc_reg CRm,
                unsigned opc2) override {
                return boost::blank{};
            }

            CallbackOrAccessTwoWords CompileGetTwoWords(bool two, unsigned opc, coproc_reg CRm) override {
                return boost::blank{};
            }

            std::optional<Callback> CompileLoadWords(bool two, bool long_transfer, coproc_reg CRd,
                std::optional<std::uint8_t> option) override {
                return std::nullopt;
            }

            std::optional<Callback> CompileStoreWords(bool two, bool long_transfer, coproc_reg CRd,
                std::optional<std::uint8_t> option) override {
                return std::nullopt;
            }
        };


        class arm_dynarmic_callback : public Dynarmic::A32::UserCallbacks {
            arm_dynarmic &parent;
            uint64_t interpreted = 0;

            bool log_read = false;
            bool log_write = false;
            bool log_code = true;

        public:
            explicit arm_dynarmic_callback(arm_dynarmic &parent)
                : parent(parent) {}

            void handle_thread_exception() {
                if (parent.mem->get_real_pointer(parent.get_pc())) {
                    const std::string disassemble_inst = parent.asmdis->disassemble(
                        reinterpret_cast<const uint8_t *>(parent.mem->get_real_pointer(parent.get_pc())),
                        (parent.jit->Cpsr() & 0x20) ? 2 : 4, parent.get_pc(),
                        (parent.jit->Cpsr() & 0x20) ? true : false);

                    LOG_TRACE("Last instruction: {} (0x{:x})", disassemble_inst, (parent.jit->Cpsr() & 0x20) ? parent.mem->read<std::uint16_t>(parent.get_pc()) : parent.mem->read<std::uint32_t>(parent.get_pc()));
                }

                if (parent.mem->get_real_pointer(parent.get_lr() - parent.get_lr() % 2)) {
                    const std::string disassemble_inst = parent.asmdis->disassemble(
                        reinterpret_cast<const uint8_t *>(parent.mem->get_real_pointer(parent.get_lr() - parent.get_lr() % 2)),
                        parent.get_lr() % 2 != 0 ? 2 : 4, parent.get_lr() - parent.get_lr() % 2,
                        parent.get_lr() % 2 != 0 ? true : false);

                    LOG_TRACE("LR instruction: {} (0x{:x})", disassemble_inst, parent.get_lr() % 2 != 0 ? parent.mem->read<std::uint16_t>(parent.get_lr() - parent.get_lr() % 2) : parent.mem->read<std::uint32_t>(parent.get_lr() - parent.get_lr() % 2));
                }

                thread_ptr crr_thread = parent.kern->crr_thread();

                crr_thread->stop();
                parent.save_context(crr_thread->get_thread_context());
                dump_context(crr_thread->get_thread_context());

                parent.stop();
                parent.debugger->wait_for_debugger();
            }

            void invalid_memory_read(const Dynarmic::A32::VAddr addr) {
                thread_ptr crr_thread = parent.kern->crr_thread();
                LOG_CRITICAL("Reading unmapped address (0x{:x}), panic thread {}", addr,
                    crr_thread->name());

                handle_thread_exception();
            }

            void invalid_memory_write(const Dynarmic::A32::VAddr addr) {
                thread_ptr crr_thread = parent.kern->crr_thread();
                LOG_CRITICAL("Writing unmapped address (0x{:x}), panic thread {}", addr,
                    crr_thread->name());

                handle_thread_exception();
            }

            void handle_read_status(const bool status, const Dynarmic::A32::VAddr addr) {
                if (!status) {
                    invalid_memory_read(addr);
                }
            }

            void handle_write_status(const bool status, const Dynarmic::A32::VAddr addr) {
                if (!status) {
                    invalid_memory_write(addr);
                }
            }

            std::uint32_t MemoryReadCode(Dynarmic::A32::VAddr addr) override {
                if (parent.debugger) {
                    bool is_thumb = parent.get_cpsr() & 0x20;
                    auto bkpt = parent.debugger->get_nearest_breakpoint(addr);

                    // 0x70BE for Thumb bkpt instruction
                    // 0x70BE20E1 for ARM bkpt instruction
                    if (bkpt && (bkpt->addr == addr) && !bkpt->is_hit) {
                        if (is_thumb) {
                            return (0xBE70 << 16) | (MemoryRead16(addr + 2));
                        }

                        return 0xE120BE70;
                    } else {
                        if (bkpt && (is_thumb) && (bkpt->addr == addr + 2)
                            && !bkpt->is_hit) {
                            return (MemoryRead16(addr) << 16) | (0xBE70);
                        }
                    }
                }

                return MemoryRead32(addr);
            }

            uint8_t MemoryRead8(Dynarmic::A32::VAddr addr) override {
                uint8_t ret = 0;
                bool success = parent.get_memory_sys()->read(addr, &ret, sizeof(uint8_t));

                if (log_read) {
                    LOG_TRACE("Read uint8_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
                }

                handle_read_status(success, addr);

                return ret;
            }

            uint16_t MemoryRead16(Dynarmic::A32::VAddr addr) override {
                uint16_t ret = 0;
                bool success = parent.get_memory_sys()->read(addr, &ret, sizeof(uint16_t));

                if (log_read) {
                    LOG_TRACE("Read uint16_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
                }

                handle_read_status(success, addr);

                return ret;
            }

            uint32_t MemoryRead32(Dynarmic::A32::VAddr addr) override {
                uint32_t ret = 0;
                bool success = parent.get_memory_sys()->read(addr, &ret, sizeof(uint32_t));

                if (log_read) {
                    LOG_TRACE("Read uint32_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
                }

                handle_read_status(success, addr);

                return ret;
            }

            uint64_t MemoryRead64(Dynarmic::A32::VAddr addr) override {
                uint64_t ret = 0;
                bool success = parent.get_memory_sys()->read(addr, &ret, sizeof(uint64_t));

                if (log_read) {
                    LOG_TRACE("Read uint64_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
                }

                handle_read_status(success, addr);

                return ret;
            }

            void MemoryWrite8(Dynarmic::A32::VAddr addr, uint8_t value) override {
                bool success = parent.get_memory_sys()->write(addr, &value, sizeof(value));

                if (log_write) {
                    LOG_TRACE("Write uint8_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
                }

                handle_write_status(success, addr);
            }

            void MemoryWrite16(Dynarmic::A32::VAddr addr, uint16_t value) override {
                bool success = parent.get_memory_sys()->write(addr, &value, sizeof(value));

                if (log_write) {
                    LOG_TRACE("Write uint16_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
                }

                handle_write_status(success, addr);
            }

            void MemoryWrite32(Dynarmic::A32::VAddr addr, uint32_t value) override {
                bool success = parent.get_memory_sys()->write(addr, &value, sizeof(value));

                if (log_write) {
                    LOG_TRACE("Write uint32_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
                }

                handle_write_status(success, addr);
            }

            void MemoryWrite64(Dynarmic::A32::VAddr addr, uint64_t value) override {
                bool success = parent.get_memory_sys()->write(addr, &value, sizeof(value));

                if (log_write) {
                    LOG_TRACE("Write uint64_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
                }

                handle_write_status(success, addr);
            }

            void InterpreterFallback(Dynarmic::A32::VAddr addr, size_t num_insts) override {
                arm_interface::thread_context context;
                parent.save_context(context);
                parent.fallback_jit.load_context(context);
                parent.fallback_jit.execute_instructions(static_cast<uint32_t>(num_insts));
                parent.fallback_jit.save_context(context);
                parent.load_context(context);

                interpreted += num_insts;
            }

            void ExceptionRaised(uint32_t pc, Dynarmic::A32::Exception exception) override {
                switch (exception) {
                case Dynarmic::A32::Exception::UndefinedInstruction: {
                    thread_ptr crr_thread = parent.kern->crr_thread();
                    LOG_TRACE("Unknown instruction by Dynarmic raised in thread {}",
                        crr_thread->name());

                    parent.debugger->wait_for_debugger();

                    break;
                }

                case Dynarmic::A32::Exception::Breakpoint: {
                    gdbstub *stub = parent.stub;

                    if (stub && stub->is_connected()) {
                        parent.jit->HaltExecution();
                        parent.set_pc(pc);

                        thread_ptr crr_thread = parent.kern->crr_thread();
                        parent.save_context(crr_thread->get_thread_context());

                        stub->break_exec();
                        stub->send_trap_gdb(crr_thread, 5);
                    }

                    if (parent.debugger) {
                        auto bkpt = parent.debugger->get_nearest_breakpoint(pc);

                        if (bkpt) {
                            parent.debugger->set_breakpoint(bkpt->addr, true);
                            LOG_TRACE("Breakpoint hit at address 0x{:x}", bkpt->addr);
                        } else {
                            LOG_TRACE("Breakpoint deleted but cache still exist, hit with pc after: 0x{:x}", pc);
                        }

                        parent.save_context(parent.kern->crr_thread()->get_thread_context());

                        parent.debugger->wait_for_debugger();

                        if (bkpt) {
                            parent.debugger->set_breakpoint(bkpt->addr, false);
                        }

                        parent.stop();
                        // Delete the cache so it will reread the instruction
                        parent.clear_instruction_cache();
                    }

                    return;
                }

                default: {
                    thread_ptr crr_thread = parent.kern->crr_thread();
                    LOG_WARN("Exception Raised at thread {}", crr_thread->name());

                    parent.debugger->wait_for_debugger();

                    break;
                }
                }

                handle_thread_exception();
            }

            void CallSVC(uint32_t svc) override {
                hle::lib_manager *mngr = parent.get_lib_manager();

                if (svc == 0x900000) {
                    uint32_t val = *reinterpret_cast<uint32_t *>(parent.get_memory_sys()->get_real_pointer(parent.get_pc() + 4));
                    bool res = mngr->call_hle(val);

                    if (!res) {
                        LOG_WARN("Unimplemented method!");
                    }

                    return;
                } else if (svc == 0x900001) {
                    // Custom call
                    uint32_t val = *reinterpret_cast<uint32_t *>(parent.get_memory_sys()->get_real_pointer(parent.get_pc() + 4));
                    bool res = mngr->call_custom_hle(val);

                    return;
                } else if (svc == 0x900002) {
                    uint32_t call_addr = parent.get_pc();

                    if (call_addr && parent.is_thumb_mode()) {
                        call_addr -= 1;
                    } else {
                        call_addr -= 4;
                    }

                    bool res = parent.get_lib_manager()->call_hle(*parent.get_lib_manager()->get_sid(call_addr));

                    if (res) {
                        uint32_t lr = parent.get_reg(14);
                        parent.set_pc(lr);
                    }

                    return;
                }

                bool res = mngr->call_svc(svc);

                if (!res) {
                    LOG_INFO("Unimplemented SVC call: 0x{:x}", svc);
                }
            }

            void AddTicks(uint64_t ticks) override {
                parent.get_timing_sys()->add_ticks(static_cast<std::uint32_t>(ticks - interpreted));
                interpreted = 0;
            }

            uint64_t GetTicksRemaining() override {
                return eka2l1::common::max(parent.get_timing_sys()->get_downcount(), 0);
            }
        };

        std::unique_ptr<Dynarmic::A32::Jit> make_jit(std::unique_ptr<arm_dynarmic_callback> &callback, void *table) {
            Dynarmic::A32::UserConfig config;
            config.callbacks = callback.get();
            config.coprocessors[15] = std::make_shared<arm_dynarmic_cp15>();
            config.page_table = reinterpret_cast<decltype(config.page_table)>(table);

            return std::make_unique<Dynarmic::A32::Jit>(config);
        }

        arm_dynarmic::arm_dynarmic(kernel_system *kern, timing_system *sys, manager_system *mngr, memory_system *mem,
            disasm *asmdis, hle::lib_manager *lmngr, gdbstub *stub, debugger_ptr debugger)
            : timing(sys)
            , mem(mem)
            , asmdis(asmdis)
            , lib_mngr(lmngr)
            , mngr(mngr)
            , stub(stub)
            , kern(kern)
            , fallback_jit(kern, sys, mngr, mem, asmdis, lmngr, stub)
            , cb(std::make_unique<arm_dynarmic_callback>(*this))
            , debugger(debugger) {
            jit = make_jit(cb, &page_dyn);
        }

        arm_dynarmic::~arm_dynarmic() {}

        void arm_dynarmic::run() {
            jit->Run();
        }

        void arm_dynarmic::stop() {
            jit->HaltExecution();
        }

        void arm_dynarmic::step() {
            cb->InterpreterFallback(get_pc(), 1);
        }

        uint32_t arm_dynarmic::get_reg(size_t idx) {
            return jit->Regs()[idx];
        }

        uint32_t arm_dynarmic::get_sp() {
            return jit->Regs()[13];
        }

        uint32_t arm_dynarmic::get_pc() {
            return jit->Regs()[15];
        }

        uint32_t arm_dynarmic::get_vfp(size_t idx) {
            return 0;
        }

        void arm_dynarmic::set_reg(size_t idx, uint32_t val) {
            jit->Regs()[idx] = val;
        }

        void arm_dynarmic::set_pc(uint32_t val) {
            jit->Regs()[15] = val;
        }

        void arm_dynarmic::set_sp(uint32_t val) {
            jit->Regs()[13] = val;
        }

        void arm_dynarmic::set_lr(uint32_t val) {
            jit->Regs()[14] = val;
        }

        void arm_dynarmic::set_vfp(size_t idx, uint32_t val) {
        }

        uint32_t arm_dynarmic::get_lr() {
            return jit->Regs()[14];
        }

        uint32_t arm_dynarmic::get_cpsr() {
            return jit->Cpsr();
        }

        void arm_dynarmic::set_cpsr(uint32_t val) {
            jit->SetCpsr(val);
        }

        void arm_dynarmic::save_context(thread_context &ctx) {
            ctx.cpsr = get_cpsr();

            for (uint8_t i = 0; i < 16; i++) {
                ctx.cpu_registers[i] = get_reg(i);
            }

            ctx.pc = get_pc();
            ctx.sp = get_sp();
            ctx.lr = get_lr();

            if (!ctx.pc) {
                LOG_WARN("Dynarmic save context with PC = 0");
            }
        }

        void arm_dynarmic::load_context(const thread_context &ctx) {
            for (uint8_t i = 0; i < 16; i++) {
                jit->Regs()[i] = ctx.cpu_registers[i];
            }

            set_sp(ctx.sp);
            set_pc(ctx.pc);
            set_lr(ctx.lr);
            set_cpsr(ctx.cpsr);
        }

        void arm_dynarmic::set_entry_point(address ep) {
        }

        address arm_dynarmic::get_entry_point() {
            return 0;
        }

        void arm_dynarmic::set_stack_top(address addr) {
            set_sp(addr);
        }

        address arm_dynarmic::get_stack_top() {
            return get_sp();
        }

        void arm_dynarmic::prepare_rescheduling() {
            if (jit->IsExecuting()) {
                jit->HaltExecution();
            }
        }

        bool arm_dynarmic::is_thumb_mode() {
            return get_cpsr() & 0x20;
        }

        void arm_dynarmic::imb_range(address start, uint32_t len) {
            jit->InvalidateCacheRange(start, len);
        }

        void arm_dynarmic::page_table_changed() {
        }

        void arm_dynarmic::map_backing_mem(address vaddr, size_t size, uint8_t *ptr, prot protection) {
            for (std::size_t i = 0; i < size / mem->get_page_size(); i++) {
                page_dyn[vaddr / mem->get_page_size() + i] = ptr + i * mem->get_page_size();
			}

            fallback_jit.map_backing_mem(vaddr, size, ptr, protection);
        }

        void arm_dynarmic::unmap_memory(address addr, size_t size) {
            for (std::size_t i = addr / mem->get_page_size(); i < (addr + size) / mem->get_page_size(); i++) {
                page_dyn[i] = nullptr;
            }

            fallback_jit.unmap_memory(addr, size);
        }

        void arm_dynarmic::clear_instruction_cache() {
            jit->ClearCache();
        }
    }
}