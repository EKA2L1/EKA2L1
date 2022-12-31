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

#include <cpu/arm_dynarmic.h>
#include <cpu/arm_utils.h>

#include <dynarmic/interface/A32/context.h>
#include <dynarmic/interface/A32/coprocessor.h>

namespace eka2l1::arm {
    class dynarmic_core_cp15 : public Dynarmic::A32::Coprocessor {
        std::uint32_t uprw;
        std::uint32_t data_sync_barrier;
        std::uint32_t data_memory_barrier;

    public:
        using coproc_reg = Dynarmic::A32::CoprocReg;

        explicit dynarmic_core_cp15()
            : uprw(0)
            , data_sync_barrier(0)
            , data_memory_barrier(0) {
        }

        ~dynarmic_core_cp15() override {
        }

        void set_uprw(const std::uint32_t uprw_val) {
            uprw = uprw_val;
        }

        const std::uint32_t get_uprw() const {
            return uprw;
        }

        std::optional<Callback> CompileInternalOperation(bool two, unsigned opc1, coproc_reg CRd,
            coproc_reg CRn, coproc_reg CRm,
            unsigned opc2) override {
            return std::nullopt;
        }

        CallbackOrAccessOneWord CompileSendOneWord(bool two, unsigned opc1, coproc_reg CRn,
            coproc_reg CRm, unsigned opc2) override {
            if (!two && (CRn == coproc_reg::C7) && (opc1 == 0) && (CRm == coproc_reg::C10)) {
                switch (opc2) {
                case 4:
                    return &data_sync_barrier;
                case 5:
                    return &data_memory_barrier;
                default:
                    return CallbackOrAccessOneWord{};
                }
            }

            return CallbackOrAccessOneWord{};
        }

        CallbackOrAccessTwoWords CompileSendTwoWords(bool two, unsigned opc, coproc_reg CRm) override {
            return CallbackOrAccessTwoWords{};
        }

        CallbackOrAccessOneWord CompileGetOneWord(bool two, unsigned opc1, coproc_reg CRn, coproc_reg CRm,
            unsigned opc2) override {
            if ((CRn == coproc_reg::C13) && (CRm == coproc_reg::C0) && (opc1 == 0) && (opc2 == 2)) {
                return &uprw;
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

    class dynarmic_core_callback : public Dynarmic::A32::UserCallbacks {
        std::shared_ptr<dynarmic_core_cp15> cp15;

        dynarmic_core &parent;
        std::uint64_t interpreted;

        core::thread_context temp_context;

    public:
        explicit dynarmic_core_callback(dynarmic_core &parent, std::shared_ptr<dynarmic_core_cp15> cp15)
            : cp15(cp15)
            , parent(parent)
            , interpreted(0) {}

        dynarmic_core_cp15 *get_cp15() {
            return cp15.get();
        }

        /**
         * @brief Raise access violation and get feedback on whether we should reaccess the address again.
         * 
         * @param addr          Address where the violation happens.
         * @param is_read       True if the raised exception is from an invalid read. Otherwise, it's from an invalid write.
         * 
         * @returns True if the handler fixed the violation and a re-read/re-write should be reissued.
         */
        bool raise_invalid_access_and_get_feedback(const Dynarmic::A32::VAddr addr, const bool is_read) {
            return parent.exception_handler(is_read ? exception_type_access_violation_read : exception_type_access_violation_write, addr);
        }

        bool handle_read_status(const bool status, const Dynarmic::A32::VAddr addr) {
            if (!status) {
                return raise_invalid_access_and_get_feedback(addr, true);
            }

            return false;
        }

        bool handle_write_status(const bool status, const Dynarmic::A32::VAddr addr) {
            if (!status) {
                return raise_invalid_access_and_get_feedback(addr, false);
            }

            return false;
        }

        std::optional<std::uint32_t> MemoryReadCode(Dynarmic::A32::VAddr addr) override {
            std::uint32_t code_result = 0;

            bool status = parent.read_code(addr, &code_result);
            if (handle_read_status(status, addr)) {
                status = parent.read_code(addr, &code_result);
            }

            return status ? std::make_optional<std::uint32_t>(code_result) : std::nullopt;
        }

        uint8_t MemoryRead8(Dynarmic::A32::VAddr addr) override {
            std::uint8_t ret = 0;
            if (handle_read_status(parent.read_8bit(addr, &ret), addr)) {
                parent.read_8bit(addr, &ret);
            }

            return ret;
        }

        uint16_t MemoryRead16(Dynarmic::A32::VAddr addr) override {
            std::uint16_t ret = 0;
            if (handle_read_status(parent.read_16bit(addr, &ret), addr)) {
                parent.read_16bit(addr, &ret);
            }

            return ret;
        }

        uint32_t MemoryRead32(Dynarmic::A32::VAddr addr) override {
            std::uint32_t ret = 0;
            if (handle_read_status(parent.read_32bit(addr, &ret), addr)) {
                parent.read_32bit(addr, &ret);
            }

            return ret;
        }

        uint64_t MemoryRead64(Dynarmic::A32::VAddr addr) override {
            std::uint64_t ret = 0;
            if (handle_read_status(parent.read_64bit(addr, &ret), addr)) {
                parent.read_64bit(addr, &ret);
            }

            return ret;
        }

        void MemoryWrite8(Dynarmic::A32::VAddr addr, uint8_t value) override {
            if (handle_write_status(parent.write_8bit(addr, &value), addr)) {
                parent.write_8bit(addr, &value);
            }
        }

        void MemoryWrite16(Dynarmic::A32::VAddr addr, uint16_t value) override {
            if (handle_write_status(parent.write_16bit(addr, &value), addr)) {
                parent.write_16bit(addr, &value);
            }
        }

        void MemoryWrite32(Dynarmic::A32::VAddr addr, uint32_t value) override {
            if (handle_write_status(parent.write_32bit(addr, &value), addr)) {
                parent.write_32bit(addr, &value);
            }
        }

        void MemoryWrite64(Dynarmic::A32::VAddr addr, uint64_t value) override {
            if (handle_write_status(parent.write_64bit(addr, &value), addr)) {
                parent.write_64bit(addr, &value);
            }
        }

        bool MemoryWriteExclusive8(Dynarmic::A32::VAddr addr, std::uint8_t value, std::uint8_t expected) override {
            return parent.exclusive_write_8bit(addr, value, expected);
        }

        bool MemoryWriteExclusive16(Dynarmic::A32::VAddr addr, std::uint16_t value, std::uint16_t expected) override {
            return parent.exclusive_write_16bit(addr, value, expected);
        }

        bool MemoryWriteExclusive32(Dynarmic::A32::VAddr addr, std::uint32_t value, std::uint32_t expected) override {
            return parent.exclusive_write_32bit(addr, value, expected);
        }

        bool MemoryWriteExclusive64(Dynarmic::A32::VAddr addr, std::uint64_t value, std::uint64_t expected) override {
            return parent.exclusive_write_64bit(addr, value, expected);
        }

        void InterpreterFallback(Dynarmic::A32::VAddr addr, size_t num_insts) override {
            if (!parent.interpreter_callback_inited) {
                parent.interpreter.read_code = parent.read_code;
                parent.interpreter.read_8bit = parent.read_8bit;
                parent.interpreter.read_16bit = parent.read_16bit;
                parent.interpreter.read_32bit = parent.read_32bit;
                parent.interpreter.read_64bit = parent.read_64bit;

                parent.interpreter.write_8bit = parent.write_8bit;
                parent.interpreter.write_16bit = parent.write_16bit;
                parent.interpreter.write_32bit = parent.write_32bit;
                parent.interpreter.write_64bit = parent.write_64bit;

                parent.interpreter.exception_handler = parent.exception_handler;
                parent.interpreter.exclusive_write_8bit = parent.exclusive_write_8bit;
                parent.interpreter.exclusive_write_16bit = parent.exclusive_write_16bit;
                parent.interpreter.exclusive_write_32bit = parent.exclusive_write_32bit;
                parent.interpreter.exclusive_write_64bit = parent.exclusive_write_64bit;
                parent.interpreter_callback_inited = true;
            }

            parent.save_context(temp_context);
            parent.interpreter.load_context(temp_context);

            parent.interpreter.run(num_insts);
            parent.interpreter.save_context(temp_context);
            parent.load_context(temp_context);

            interpreted += num_insts;
        }

        void ExceptionRaised(uint32_t pc, Dynarmic::A32::Exception exception) override {
            switch (exception) {
            case Dynarmic::A32::Exception::UndefinedInstruction: {
                parent.exception_handler(exception_type_undefined_inst, pc);
                break;
            }

            case Dynarmic::A32::Exception::Breakpoint: {
                parent.exception_handler(exception_type_breakpoint, pc);
                return;
            }

            case Dynarmic::A32::Exception::UnpredictableInstruction:
                parent.exception_handler(exception_type_unpredictable, pc);
                return;

            default: {
                parent.exception_handler(exception_type_unk, pc);
                break;
            }
            }
        }

        void CallSVC(std::uint32_t svc) override {
            parent.system_call_handler(svc);
        }

        void AddTicks(uint64_t ticks) override {
            parent.ticks_executed += static_cast<std::uint32_t>(ticks - interpreted);
            interpreted = 0;
        }

        std::uint64_t GetTicksRemaining() override {
            return static_cast<std::uint64_t>(common::max<std::int64_t>(static_cast<std::int64_t>(parent.ticks_target)
                    - parent.ticks_executed,
                0));
        }
    };

    std::unique_ptr<Dynarmic::A32::Jit> make_jit(std::unique_ptr<dynarmic_core_callback> &callback, Dynarmic::TLB<9> &tlb_obj,
        std::shared_ptr<dynarmic_core_cp15> cp15, Dynarmic::ExclusiveMonitor *monitor) {
        Dynarmic::A32::UserConfig config;
        config.callbacks = callback.get();
        config.coprocessors[15] = cp15;
        config.tlb_entries = tlb_obj.entries.data();
        config.tlb_index_mask_bits = 9;
        config.global_monitor = monitor;
        config.define_unpredictable_behaviour = true;
        config.arch_version = Dynarmic::A32::ArchVersion::v6T2;

        return std::make_unique<Dynarmic::A32::Jit>(config);
    }

    dynarmic_core::dynarmic_core(arm::exclusive_monitor *monitor)
        : tlb_obj(12)
        , interpreter(monitor, 12)
        , interpreter_callback_inited(false) {
        std::shared_ptr<dynarmic_core_cp15> cp15 = std::make_shared<dynarmic_core_cp15>();
        cb = std::make_unique<dynarmic_core_callback>(*this, cp15);

        auto monitor_bb = reinterpret_cast<dynarmic_exclusive_monitor *>(monitor);

        jit = make_jit(cb, tlb_obj, cp15, &monitor_bb->monitor_);
    }

    dynarmic_core::~dynarmic_core() {
    }

    void dynarmic_core::run(const std::uint32_t instruction_count) {
        ticks_executed = 0;
        ticks_target = instruction_count;

        jit->Run();
    }

    void dynarmic_core::stop() {
        jit->HaltExecution();
    }

    void dynarmic_core::step() {
        jit->Step();
    }

    uint32_t dynarmic_core::get_reg(size_t idx) {
        return jit->Regs()[idx];
    }

    uint32_t dynarmic_core::get_sp() {
        return jit->Regs()[13];
    }

    uint32_t dynarmic_core::get_pc() {
        return jit->Regs()[15];
    }

    uint32_t dynarmic_core::get_vfp(size_t idx) {
        return jit->ExtRegs()[idx];
    }

    void dynarmic_core::set_reg(size_t idx, uint32_t val) {
        jit->Regs()[idx] = val;
    }

    void dynarmic_core::set_pc(uint32_t val) {
        jit->Regs()[15] = val;
    }

    void dynarmic_core::set_sp(uint32_t val) {
        jit->Regs()[13] = val;
    }

    void dynarmic_core::set_lr(uint32_t val) {
        jit->Regs()[14] = val;
    }

    void dynarmic_core::set_vfp(size_t idx, uint32_t val) {
        jit->ExtRegs()[idx] = val;
    }

    uint32_t dynarmic_core::get_lr() {
        return jit->Regs()[14];
    }

    uint32_t dynarmic_core::get_cpsr() {
        return jit->Cpsr();
    }

    uint32_t dynarmic_core::get_fpscr() {
        return jit->Fpscr();
    }

    void dynarmic_core::set_cpsr(uint32_t val) {
        jit->SetCpsr(val);
    }

    void dynarmic_core::set_fpscr(uint32_t val) {
        jit->SetFpscr(val);
    }

    void dynarmic_core::save_context(thread_context &ctx) {
        ctx.cpsr = get_cpsr();
        ctx.fpscr = get_fpscr();

        for (uint8_t i = 0; i < 16; i++) {
            ctx.cpu_registers[i] = get_reg(i);
        }

        for (uint8_t i = 0; i < 64; i++) {
            ctx.fpu_registers[i] = get_vfp(i);
        }

        ctx.uprw = cb->get_cp15()->get_uprw();
    }

    void dynarmic_core::load_context(const thread_context &ctx) {
        for (uint8_t i = 0; i < 16; i++) {
            jit->Regs()[i] = ctx.cpu_registers[i];
        }

        for (uint8_t i = 0; i < 64; i++) {
            jit->ExtRegs()[i] = ctx.fpu_registers[i];
        }

        set_cpsr(ctx.cpsr);
        set_fpscr(ctx.fpscr);

        cb->get_cp15()->set_uprw(ctx.uprw);
    }

    bool dynarmic_core::is_thumb_mode() {
        return get_cpsr() & 0x20;
    }

    void dynarmic_core::set_tlb_page(address vaddr, std::uint8_t *ptr, prot protection) {
        Dynarmic::MemoryPermission prot_flags = Dynarmic::MemoryPermission::Read;
        switch (protection) {
        case prot_read:
            prot_flags |= Dynarmic::MemoryPermission::Read;
            break;

        case prot_read_write:
            prot_flags |= Dynarmic::MemoryPermission::ReadWrite;
            break;

        case prot_read_exec:
            prot_flags |= (Dynarmic::MemoryPermission::Read | Dynarmic::MemoryPermission::Execute);
            break;

        case prot_read_write_exec:
            prot_flags |= (Dynarmic::MemoryPermission::Read | Dynarmic::MemoryPermission::Write | Dynarmic::MemoryPermission::Execute);
            break;

        default:
            break;
        }

        tlb_obj.Add(vaddr, ptr, prot_flags);
    }

    void dynarmic_core::dirty_tlb_page(address addr) {
        tlb_obj.MakeDirty(addr);
    }

    void dynarmic_core::flush_tlb() {
        tlb_obj.Flush();
    }

    void dynarmic_core::clear_instruction_cache() {
        jit->ClearCache();
    }

    void dynarmic_core::imb_range(address addr, std::size_t size) {
        jit->InvalidateCacheRange(addr, size);
    }

    std::uint32_t dynarmic_core::get_num_instruction_executed() {
        return ticks_executed;
    }

    dynarmic_exclusive_monitor::dynarmic_exclusive_monitor(const std::size_t processor_count)
        : monitor_(processor_count) {
    }

    std::uint8_t dynarmic_exclusive_monitor::exclusive_read8(core *cc, address vaddr) {
        return monitor_.ReadAndMark<std::uint8_t>(cc->core_number(), vaddr, [&]() -> std::uint8_t {
            // TODO: Access violation if there is
            std::uint8_t val = 0;
            read_8bit(cc, vaddr, &val);

            return val;
        });
    }

    std::uint16_t dynarmic_exclusive_monitor::exclusive_read16(core *cc, address vaddr) {
        return monitor_.ReadAndMark<std::uint16_t>(cc->core_number(), vaddr, [&]() -> std::uint16_t {
            // TODO: Access violation if there is
            std::uint16_t val = 0;
            read_16bit(cc, vaddr, &val);

            return val;
        });
    }

    std::uint32_t dynarmic_exclusive_monitor::exclusive_read32(core *cc, address vaddr) {
        return monitor_.ReadAndMark<std::uint32_t>(cc->core_number(), vaddr, [&]() -> std::uint32_t {
            // TODO: Access violation if there is
            std::uint32_t val = 0;
            read_32bit(cc, vaddr, &val);

            return val;
        });
    }

    std::uint64_t dynarmic_exclusive_monitor::exclusive_read64(core *cc, address vaddr) {
        return monitor_.ReadAndMark<std::uint64_t>(cc->core_number(), vaddr, [&]() -> std::uint64_t {
            // TODO: Access violation if there is
            std::uint64_t val = 0;
            read_64bit(cc, vaddr, &val);

            return val;
        });
    }

    void dynarmic_exclusive_monitor::clear_exclusive() {
        monitor_.Clear();
    }

    bool dynarmic_exclusive_monitor::exclusive_write8(core *cc, address vaddr, std::uint8_t value) {
        return monitor_.DoExclusiveOperation<std::uint8_t>(cc->core_number(), vaddr, [&](std::uint8_t expected) -> bool {
            const std::int32_t res = write_8bit(cc, vaddr, value, expected);

            // TODO: Parse access violation errors
            return res > 0;
        });
    }

    bool dynarmic_exclusive_monitor::exclusive_write16(core *cc, address vaddr, std::uint16_t value) {
        return monitor_.DoExclusiveOperation<std::uint16_t>(cc->core_number(), vaddr, [&](std::uint16_t expected) -> bool {
            const std::int32_t res = write_16bit(cc, vaddr, value, expected);

            // TODO: Parse access violation errors
            return res > 0;
        });
    }

    bool dynarmic_exclusive_monitor::exclusive_write32(core *cc, address vaddr, std::uint32_t value) {
        return monitor_.DoExclusiveOperation<std::uint32_t>(cc->core_number(), vaddr, [&](std::uint32_t expected) -> bool {
            const std::int32_t res = write_32bit(cc, vaddr, value, expected);

            // TODO: Parse access violation errors
            return res > 0;
        });
    }

    bool dynarmic_exclusive_monitor::exclusive_write64(core *cc, address vaddr, std::uint64_t value) {
        return monitor_.DoExclusiveOperation<std::uint64_t>(cc->core_number(), vaddr, [&](std::uint64_t expected) -> bool {
            const std::int32_t res = write_64bit(cc, vaddr, value, expected);

            // TODO: Parse access violation errors
            return res > 0;
        });
    }
}
