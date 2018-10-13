#include <cstdint>

#include <core/arm/jit_utils.h>
#include <common/log.h>

namespace eka2l1::arm {
    void dump_context(jit_interface::thread_context uni) {
        LOG_TRACE("Dumping CPU context: ");
        LOG_TRACE("pc: 0x{:x}", uni.pc);
        LOG_TRACE("lr: 0x{:x}", uni.lr);
        LOG_TRACE("sp: 0x{:x}", uni.sp);
        LOG_TRACE("cpsr: 0x{:x}", uni.cpsr);

        for (std::size_t i = 0; i < uni.cpu_registers.size(); i++) {
            LOG_TRACE("r{}: 0x{:x}", i, uni.cpu_registers[i]);
        }
    }
}