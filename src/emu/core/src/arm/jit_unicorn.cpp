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
        }

        jit_unicorn::~jit_unicorn() {
            uc_close(engine);
        }

        void execute_instructions(uc_engine* engine, uint64_t pc, int num_instructions) {
            uc_emu_start(engine, pc, 1ULL << 63, 0, num_instructions);
        }

        void jit_unicorn::run() {
            execute_instructions(engine, get_pc(), std::max(core_timing::get_downcount(), 0));
        }

        void jit_unicorn::stop() {
            uc_emu_stop(engine);
        }

        void jit_unicorn::step() {
            execute_instructions(engine, get_pc(), 1);
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

        void jit_unicorn::set_lr(uint64_t val) {

        }

        void jit_unicorn::set_vfp(size_t idx, uint64_t val) {

        }

        uint32_t jit_unicorn::get_cpsr() {
            return 0;
        }

        void jit_unicorn::save_context(thread_context& ctx) {

        }

        void jit_unicorn::load_context(const thread_context& ctx) {

        }
    }
}
