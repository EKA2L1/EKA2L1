#include <arm/jit_factory.h>
#include <arm/jit_unicorn.h>

namespace eka2l1 {
    namespace arm {
        jitter create_jitter(jitter_arm_type arm_type) {
            switch (arm_type) {
            case unicorn:
                return std::make_unique<jit_unicorn>();
            default:
                return jitter(nullptr);
            }
        }
    }
}
