/*
 * Copyright (c) 2018 EKA2L1 Team.
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
#include <common/configure.h>
#include <common/log.h>

#include <arm/arm_dynarmic.h>
#include <arm/arm_utils.h>
#include <disasm/disasm.h>

#include <kernel/kernel.h>
#include <kernel/libmanager.h>
#include <mem/mem.h>
#include <kernel/timing.h>

#include <gdbstub/gdbstub.h>

#include <dynarmic/A32/context.h>
#include <dynarmic/A32/coprocessor.h>

#include <manager/config.h>

#if ENABLE_SCRIPTING
#include <manager/manager.h>
#include <manager/script_manager.h>
#endif

namespace eka2l1::arm {
    class arm_dynarmic_cp15 : public Dynarmic::A32::Coprocessor {
        std::uint32_t wrwr;

    public:
        using coproc_reg = Dynarmic::A32::CoprocReg;

        explicit arm_dynarmic_cp15()
            : wrwr(0) {
        }

        ~arm_dynarmic_cp15() override {
        }

        void set_wrwr(const std::uint32_t wrwr_val) {
            wrwr = wrwr_val;
        }

        const std::uint32_t get_wrwr() const {
            return wrwr;
        }

        std::optional<Callback> CompileInternalOperation(bool two, unsigned opc1, coproc_reg CRd,
            coproc_reg CRn, coproc_reg CRm,
            unsigned opc2) override {
            return std::nullopt;
        }

        CallbackOrAccessOneWord CompileSendOneWord(bool two, unsigned opc1, coproc_reg CRn,
            coproc_reg CRm, unsigned opc2) override {
            return CallbackOrAccessOneWord{};
        }

        CallbackOrAccessTwoWords CompileSendTwoWords(bool two, unsigned opc, coproc_reg CRm) override {
            return CallbackOrAccessTwoWords{};
        }

        CallbackOrAccessOneWord CompileGetOneWord(bool two, unsigned opc1, coproc_reg CRn, coproc_reg CRm,
            unsigned opc2) override {
            if ((CRn == coproc_reg::C13) && (CRm == coproc_reg::C0) && (opc1 == 0) && (opc2 == 2)) {
                return &wrwr;
            }

            return CallbackOrAccessOneWord{};
        }

        CallbackOrAccessTwoWords CompileGetTwoWords(bool two, unsigned opc, coproc_reg CRm) override {
            return CallbackOrAccessTwoWords{};
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
        std::shared_ptr<arm_dynarmic_cp15> cp15;

        arm_dynarmic &parent;
        std::uint64_t interpreted;

    public:
        explicit arm_dynarmic_callback(arm_dynarmic &parent, std::shared_ptr<arm_dynarmic_cp15> cp15)
            : cp15(cp15)
            , parent(parent)
            , interpreted(0) {}

        arm_dynarmic_cp15 *get_cp15() {
            return cp15.get();
        }

        void handle_thread_exception() {
            kernel::thread *crr_thread = parent.kern->crr_thread();
            kernel::process *pr = crr_thread->owning_process();
            std::uint8_t *pc_data = reinterpret_cast<std::uint8_t *>(pr->get_ptr_on_addr_space(parent.get_pc()));

            if (pc_data) {
                const std::string disassemble_inst = parent.asmdis->disassemble(pc_data,
                    (parent.jit->Cpsr() & 0x20) ? 2 : 4, parent.get_pc(),
                    (parent.jit->Cpsr() & 0x20) ? true : false);

                LOG_TRACE("Last instruction: {} (0x{:x})", disassemble_inst, (parent.jit->Cpsr() & 0x20)
                        ? *reinterpret_cast<std::uint16_t *>(pc_data)
                        : *reinterpret_cast<std::uint32_t *>(pc_data));
            }

            pc_data = reinterpret_cast<std::uint8_t *>(pr->get_ptr_on_addr_space(parent.get_lr()));

            if (pc_data) {
                const std::string disassemble_inst = parent.asmdis->disassemble(pc_data,
                    parent.get_lr() % 2 != 0 ? 2 : 4, parent.get_lr() - parent.get_lr() % 2,
                    parent.get_lr() % 2 != 0 ? true : false);

                LOG_TRACE("LR instruction: {} (0x{:x})", disassemble_inst, (parent.get_lr() % 2 != 0)
                        ? *reinterpret_cast<std::uint16_t *>(pc_data)
                        : *reinterpret_cast<std::uint32_t *>(pc_data));
            }

            parent.stop();

            // Dump thread contexts
            crr_thread->stop();
            parent.save_context(crr_thread->get_thread_context());
            dump_context(crr_thread->get_thread_context());
        }

        void invalid_memory_read(const Dynarmic::A32::VAddr addr) {
            kernel::thread *crr_thread = parent.kern->crr_thread();
            LOG_CRITICAL("Reading unmapped address (0x{:x}), panic thread {}", addr,
                crr_thread->name());

            handle_thread_exception();
        }

        void invalid_memory_write(const Dynarmic::A32::VAddr addr) {
            kernel::thread *crr_thread = parent.kern->crr_thread();
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
            void *data = parent.kern->crr_process()->get_ptr_on_addr_space(addr);
            if (!data) {
                invalid_memory_read(addr);
                return 0;
            }

            std::uint32_t code = 0;
            std::memcpy(&code, data, sizeof(std::uint32_t));

            return code;
        }

        uint8_t MemoryRead8(Dynarmic::A32::VAddr addr) override {
            uint8_t ret = 0;
            bool success = parent.get_memory_sys()->read(addr, &ret, sizeof(uint8_t));

            if (parent.conf->log_read) {
                LOG_TRACE("Read uint8_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
            }

            handle_read_status(success, addr);

            return ret;
        }

        uint16_t MemoryRead16(Dynarmic::A32::VAddr addr) override {
            uint16_t ret = 0;
            bool success = parent.get_memory_sys()->read(addr, &ret, sizeof(uint16_t));

            if (parent.conf->log_read) {
                LOG_TRACE("Read uint16_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
            }

            handle_read_status(success, addr);

            return ret;
        }

        uint32_t MemoryRead32(Dynarmic::A32::VAddr addr) override {
            uint32_t ret = 0;
            bool success = parent.get_memory_sys()->read(addr, &ret, sizeof(uint32_t));

            if (parent.conf->log_read) {
                LOG_TRACE("Read uint32_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
            }

            handle_read_status(success, addr);

            return ret;
        }

        uint64_t MemoryRead64(Dynarmic::A32::VAddr addr) override {
            uint64_t ret = 0;
            bool success = parent.get_memory_sys()->read(addr, &ret, sizeof(uint64_t));

            if (parent.conf->log_read) {
                LOG_TRACE("Read uint64_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
            }

            handle_read_status(success, addr);

            return ret;
        }

        void MemoryWrite8(Dynarmic::A32::VAddr addr, uint8_t value) override {
            bool success = parent.get_memory_sys()->write(addr, &value, sizeof(value));

            if (parent.conf->log_write) {
                LOG_TRACE("Write uint8_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
            }

            handle_write_status(success, addr);
        }

        void MemoryWrite16(Dynarmic::A32::VAddr addr, uint16_t value) override {
            bool success = parent.get_memory_sys()->write(addr, &value, sizeof(value));

            if (parent.conf->log_write) {
                LOG_TRACE("Write uint16_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
            }

            handle_write_status(success, addr);
        }

        void MemoryWrite32(Dynarmic::A32::VAddr addr, uint32_t value) override {
            bool success = parent.get_memory_sys()->write(addr, &value, sizeof(value));

            if (parent.conf->log_write) {
                LOG_TRACE("Write uint32_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
            }

            handle_write_status(success, addr);
        }

        void MemoryWrite64(Dynarmic::A32::VAddr addr, uint64_t value) override {
            bool success = parent.get_memory_sys()->write(addr, &value, sizeof(value));

            if (parent.conf->log_write) {
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
                kernel::thread *crr_thread = parent.kern->crr_thread();
                LOG_TRACE("Unknown instruction by Dynarmic raised in thread {}",
                    crr_thread->name());

                break;
            }

            case Dynarmic::A32::Exception::Breakpoint: {
                parent.handle_breakpoint(parent.kern, parent.conf, pc);
                return;
            }

            default: {
                kernel::thread *crr_thread = parent.kern->crr_thread();
                LOG_WARN("Exception Raised at thread {}", crr_thread->name());

                break;
            }
            }

            handle_thread_exception();
        }

        void CallSVC(std::uint32_t svc) override {
            hle::lib_manager *mngr = parent.get_lib_manager();
            bool res = mngr->call_svc(svc);

            if (!res) {
                LOG_INFO("Unimplemented SVC call: 0x{:x}", svc);
            }
        }

        void AddTicks(uint64_t ticks) override {
            parent.ticks_executed += static_cast<std::uint32_t>(ticks - interpreted);
            interpreted = 0;
        }

        std::uint64_t GetTicksRemaining() override {
            return static_cast<std::uint64_t>(common::max<std::int64_t>(static_cast<std::int64_t>(parent.ticks_target)
                    - parent.ticks_executed, 0));
        }
    };

    std::unique_ptr<Dynarmic::A32::Jit> make_jit(std::unique_ptr<arm_dynarmic_callback> &callback, void *table,
        std::shared_ptr<arm_dynarmic_cp15> cp15) {
        Dynarmic::A32::UserConfig config;
        config.callbacks = callback.get();
        config.coprocessors[15] = cp15;
        config.page_table = reinterpret_cast<decltype(config.page_table)>(table);

        return std::make_unique<Dynarmic::A32::Jit>(config);
    }

    arm_dynarmic::arm_dynarmic(kernel_system *kern, ntimer *sys, manager::config_state *conf,
        manager_system *mngr, memory_system *mem, disasm *asmdis, hle::lib_manager *lmngr, gdbstub *stub)
        : arm_interface_extended(stub, mngr)
        , timing(sys)
        , mem(mem)
        , asmdis(asmdis)
        , lib_mngr(lmngr)
        , conf(conf)
        , kern(kern)
        , fallback_jit(kern, sys, conf, mngr, mem, asmdis, lmngr, stub) {
        std::shared_ptr<arm_dynarmic_cp15> cp15 = std::make_shared<arm_dynarmic_cp15>();
        cb = std::make_unique<arm_dynarmic_callback>(*this, cp15);

        jit = make_jit(cb, &page_dyn, cp15);
    }

    arm_dynarmic::~arm_dynarmic() {}

    void arm_dynarmic::run(const std::uint32_t instruction_count) {
        ticks_executed = 0;
        ticks_target = instruction_count;

        jit->Run();
    }

    void arm_dynarmic::stop() {
        jit->HaltExecution();
    }

    void arm_dynarmic::step() {
        jit->Step();
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

        ctx.wrwr = cb->get_cp15()->get_wrwr();
    }

    void arm_dynarmic::load_context(const thread_context &ctx) {
        for (uint8_t i = 0; i < 16; i++) {
            jit->Regs()[i] = ctx.cpu_registers[i];
        }

        set_sp(ctx.sp);
        set_pc(ctx.pc);
        set_lr(ctx.lr);
        set_cpsr(ctx.cpsr);

        cb->get_cp15()->set_wrwr(ctx.wrwr);
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

    void arm_dynarmic::page_table_changed() {
    }

    void arm_dynarmic::map_backing_mem(address vaddr, size_t size, uint8_t *ptr, prot protection) {
        const std::uint32_t psize = mem->get_page_size();
        const std::uint32_t pstart = vaddr / psize;

        for (std::size_t i = 0; i < size / psize; i++) {
            page_dyn[pstart + i] = ptr + i * psize;
        }

        // fallback_jit.map_backing_mem(vaddr, size, ptr, protection);
    }

    void arm_dynarmic::unmap_memory(address addr, size_t size) {
        const std::uint32_t psize = mem->get_page_size();
        const std::uint32_t pstart = addr / psize;

        for (std::size_t i = 0; i < size / psize; i++) {
            page_dyn[pstart + i] = nullptr;
        }

        // fallback_jit.unmap_memory(addr, size);
    }

    void arm_dynarmic::clear_instruction_cache() {
        jit->ClearCache();
    }

    void arm_dynarmic::imb_range(address addr, std::size_t size) {
        jit->InvalidateCacheRange(addr, size);
    }

    std::uint32_t arm_dynarmic::get_num_instruction_executed() {
        return ticks_executed;
    }
}
