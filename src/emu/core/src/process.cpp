#include <core_kernel.h>
#include <process.h>

namespace eka2l1 {
    process::process(uint32_t uid,
        const std::string &process_name, uint32_t epa, size_t min_heap_size,
        size_t max_heap_size, size_t stack_size,
        arm::jitter_arm_type arm_type)
        : uid(uid)
        , process_name(process_name)
        , prthr(process_name, epa, stack_size, min_heap_size, max_heap_size,
              nullptr, kernel::priority_normal, arm_type) {
    }

    // Create a new thread and run
    // No arguments provided
    bool process::run() {
        return core_kernel::run_thread(prthr.unique_id());
    }
}
