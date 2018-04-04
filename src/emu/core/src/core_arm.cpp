#include <core/core_arm.h>
#include <core/arm/jit_unicorn.h>

namespace eka2l1 {
    namespace core_arm {
        std::shared_ptr<arm::jit_interface> jit_factory(core_arm_type arm_type) {
            switch (arm_type) {
                case core_arm_type::unicorn:
                    return std::shared_ptr<arm::jit_unicorn>();

                default:
                    return nullptr;
            }
        }

        void init(core_arm_type arm_type) {

        }
    }
}
