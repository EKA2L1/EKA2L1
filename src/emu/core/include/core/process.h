#pragma once

#include <kernel/thread.h>
#include <loader/eka2img.h>
#include <string>

namespace eka2l1 {
    class kernel_system;
    class memory;

	struct process_info {
		ptr<void> code_where;
		uint64_t size;
	};
    
    class process {
        uint32_t uid;
        std::string process_name;
        kernel::thread prthr;
        kernel_system* kern;
        memory_system* mem;

		loader::eka2img img;

    public:
        process() = default;
        process(kernel_system* kern, memory_system* mem, uint32_t uid,
            const std::string &process_name, loader::eka2img& img);

        // Create a new thread and run
        // No arguments provided
        bool run();

        // Step through instructions
        bool step();

        // Stop the main process thread
        bool stop();
    };
}
