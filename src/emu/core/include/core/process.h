#pragma once

#include <kernel/thread.h>
#include <string>

namespace eka2l1 {
    class process {
        uint32_t uid;
        std::string process_name;
        kernel::thread prthr;

    public:
        process() = default;
        process(uint32_t uid,
            const std::string &process_name, uint32_t epa, size_t min_heap_size,
            size_t max_heap_size, size_t stack_size,
            arm::jitter_arm_type arm_type = arm::unicorn);

        // Create a new thread and run
        // No arguments provided
        bool run();

        // Step through instructions
        bool step();

        // Stop the main process thread
        bool stop();
    };
}
