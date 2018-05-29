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

	namespace loader {
		using e32img_ptr = std::shared_ptr<eka2img>;
	}
    
    class process {
        uint32_t uid;
        std::string process_name;
        kernel::thread prthr;
        kernel_system* kern;
        memory_system* mem;

		loader::e32img_ptr img;

		std::vector<kernel::thread*> own_threads;

    public:
        process() = default;
        process(kernel_system* kern, memory_system* mem, uint32_t uid,
            const std::string &process_name, loader::e32img_ptr& img);

		uint32_t get_uid() {
			return uid;
		}

		loader::e32img_ptr get_e32img() {
			return img;
		}

        // Create a new thread and run
        // No arguments provided
        bool run();

        // Step through instructions
        bool step();

		// LOL
		bool suspend() { return true; }

        // Stop the main process thread
        bool stop();
    };
}
