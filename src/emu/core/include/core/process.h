#pragma once

#include <kernel/thread.h>
#include <string>

namespace eka2l1 {
    class kernel_system;
    class memory;
    
    class process {
        uint32_t uid;
        std::string process_name;
        kernel::thread prthr;
        kernel_system* kern;
        memory_system* mem;

    public:
        process() = default;
        process(kernel_system* kern, memory_system* mem, uint32_t uid,
            const std::string &process_name, uint32_t epa, size_t min_heap_size,
            size_t max_heap_size, size_t stack_size);

        // Create a new thread and run
        // No arguments provided
        bool run();

        // Step through instructions
        bool step();

        // Stop the main process thread
        bool stop();
    };
}
