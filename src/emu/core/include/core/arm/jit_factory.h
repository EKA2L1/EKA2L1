#pragma once

#include <memory>

namespace eka2l1 {
    namespace arm {
        enum jitter_arm_type {
            unicorn = 0
        };

        class jit_interface;
        using jitter = std::unique_ptr<jit_interface>;

        // Create a jitter. A JITter is unique by itself.
        jitter create_jitter(jitter_arm_type arm_type);
    }
}
