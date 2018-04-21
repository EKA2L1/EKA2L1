#include <arm/jit_unicorn.h>
#include <common/types.h>
#include <common/log.h>
#include <common/algorithm.h>
#include <disasm/disasm.h>
#include <core_timing.h>
#include <ptr.h>
#include <cassert>

#include <unicorn/unicorn.h>

bool thumb_mode(uc_engine *uc) {
    size_t mode = 0;
    auto err = uc_query(uc, UC_QUERY_MODE, &mode);

    if (err != UC_ERR_OK) {
        LOG_INFO("Query mode failed");
        return false;
    }

    return mode & UC_MODE_THUMB;
}

void read_hook(uc_engine *uc, uc_mem_type type, uint32_t address, int size, int64_t value, void *user_data) {
    memcpy(&value, eka2l1::ptr<const void>(address).get(), size);
    LOG_TRACE("Read at address = {}, size = 0x{:x}, val = 0x{:x}"
              , address, size, value);
}

void code_hook(uc_engine *uc, uint32_t address, uint32_t size, void *user_data) {
   const uint8_t *const code = eka2l1::ptr<const uint8_t>(address).get();
   const size_t buffer_size = eka2l1::common::GB(4) - address;
   const bool thumb = thumb_mode(uc);
   const std::string disassembly = eka2l1::disasm::disassemble(code, buffer_size, address, thumb);
   LOG_TRACE("{:#08x} {}", address, disassembly);
}

// Read the symbol and redirect to HLE function
void intr_hook(uc_engine* uc, uint32_t in_no, void* user_data) {

}

namespace eka2l1 {
    namespace arm {
        void enable_vfp_fp(uc_engine* en) {
            uint64_t c1c0  = 0;
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

        jit_unicorn::jit_unicorn() {
            uc_err err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &engine);
            assert(err == UC_ERR_OK);

            uc_hook hook {};

            uc_hook_add(engine, &hook, UC_HOOK_MEM_READ, reinterpret_cast<void *>(read_hook), nullptr, 1, 0);
            uc_hook_add(engine, &hook, UC_HOOK_CODE, reinterpret_cast<void*>(code_hook), nullptr, 1, 0);

            // Map for unicorn to run around and play
            // Sure it won't die
            // Haha
            // Haha
            err = uc_mem_map_ptr(engine, 0, common::GB(4), UC_PROT_ALL, core_mem::memory.get());
                assert(err == UC_ERR_OK);

            enable_vfp_fp(engine);
        }

        jit_unicorn::~jit_unicorn() {
            uc_close(engine);
        }

        bool jit_unicorn::execute_instructions(int num_instructions) {
            uint32_t pc = get_pc();
            bool tm = thumb_mode(engine);
            if (tm) {
                pc |= 1;
            }

            uc_reg_write(engine, UC_ARM_REG_LR, &epa);

            int err = uc_emu_start(engine, pc, 0, 0, 1);
            pc = get_pc();

            tm = thumb_mode(engine);

            if (tm) {
                pc |= 1;
            }

            err = uc_emu_start(engine, pc, epa & 0xfffffffe, 0, num_instructions - 1);

            assert(err == UC_ERR_OK);

            pc = get_pc();

            tm = thumb_mode(engine);

            if (tm) {
                pc |= 1;
            }

            core_timing::add_ticks(num_instructions);

            return pc == epa;
        }

        void jit_unicorn::run() {
            execute_instructions(std::max(core_timing::get_downcount(), 0));
        }

        void jit_unicorn::stop() {
            uc_emu_stop(engine);
        }

        void jit_unicorn::step() {
            execute_instructions(1);
        }

        uint32_t jit_unicorn::get_reg(size_t idx) {
            uint32_t val = 0;
            auto treg = UC_ARM_REG_SP + UC_ARM_REG_R0 + idx;
            auto err = uc_reg_read(engine, treg, &val);

            if (err != UC_ERR_OK) {
                LOG_ERROR("Failed to get ARM CPU registers.");
            }

            return val;
        }

        uint64_t jit_unicorn::get_sp() {
            uint64_t ret = 0;
            auto err = uc_reg_read(engine, UC_ARM_REG_SP, &ret);

            if (err != UC_ERR_OK) {
                LOG_WARN("Failed to read ARM CPU SP!");
            }

            return ret;
        }

        uint64_t jit_unicorn::get_pc() {
            uint32_t ret = 0;
            auto err = uc_reg_read(engine, UC_ARM_REG_PC, &ret);

            if (err != UC_ERR_OK) {
                LOG_WARN("Failed to read ARM CPU PC!");
            }

            return ret;
        }

        uint64_t jit_unicorn::get_vfp(size_t idx) {
            uint64_t temp;

            return temp;
        }

        void jit_unicorn::set_reg(size_t idx, uint32_t val) {
            auto treg = UC_ARM_REG_SP + UC_ARM_REG_R0 + idx;
            auto err = uc_reg_read(engine, treg, &val);

            if (err != UC_ERR_OK) {
                LOG_ERROR("Failed to set ARM CPU registers.");
            }
        }

        void jit_unicorn::set_pc(uint64_t pc) {
            auto err = uc_reg_write(engine, UC_ARM_REG_PC, &pc);

            if (err != UC_ERR_OK) {
                LOG_WARN("ARM PC failed to be set!");
            }
        }

        void jit_unicorn::set_entry_point(address ep) {
            epa = ep;
        }

        address jit_unicorn::get_entry_point() {
            return epa;
        }

        void jit_unicorn::set_lr(uint64_t val) {
            auto err = uc_reg_write(engine, UC_ARM_REG_LR, &val);

            if (err != UC_ERR_OK) {
                LOG_WARN("ARM PC failed to be set!");
            }
        }

        void jit_unicorn::set_stack_top(address addr) {
            auto err = uc_reg_write(engine, UC_ARM_REG_SP, &addr);

            if (err != UC_ERR_OK) {
                LOG_WARN("ARM PC failed to be set!");
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

        void jit_unicorn::set_vfp(size_t idx, uint64_t val) {

        }

        uint32_t jit_unicorn::get_cpsr() {
            return 0;
        }

        void jit_unicorn::save_context(thread_context& ctx) {
            for (auto i = 0; i < ctx.cpu_registers.size(); i++) {
                ctx.cpu_registers[i] = get_reg(i);
            }

            ctx.pc = get_pc();
            ctx.sp = get_sp();
        }

        void jit_unicorn::load_context(const thread_context& ctx) {
            set_pc(ctx.pc);
            //set_sp(ctx.sp);

            for (auto i = 0; i <ctx.cpu_registers.size(); i++) {
                set_reg(i, ctx.cpu_registers[i]);
            }
        }
    }
}
