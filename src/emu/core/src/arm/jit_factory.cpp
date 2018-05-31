#include <arm/jit_factory.h>
#include <arm/jit_unicorn.h>
#include <core_timing.h>

namespace eka2l1 {
    namespace arm {
        jitter create_jitter(timing_system *timing, memory_system *mem,
            disasm *asmdis, hle::lib_manager *mngr, jitter_arm_type arm_type) {
            switch (arm_type) {
            case unicorn:
                return std::make_unique<jit_unicorn>(timing, mem, asmdis, mngr);
            default:
                return jitter(nullptr);
            }
        }
    }
}
