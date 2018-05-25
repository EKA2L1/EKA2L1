#include <core_kernel.h>
#include <process.h>

namespace eka2l1 {
    process::process(kernel_system* kern, memory_system* mem, uint32_t uid,
        const std::string &process_name, loader::eka2img& img)
        : uid(uid)
        , process_name(process_name)
        , prthr(kern, mem, process_name, img.rt_code_addr + img.header.entry_point, 
			img.header.stack_size, img.header.heap_size_min, img.header.heap_size_max,
            nullptr, kernel::priority_normal)
        , kern(kern)
        , mem(mem)
		, img(img) {
    }

	bool process::stop() {
		prthr.stop();
		mem->free(img.header.code_offset);

		return true;
	}

    // Create a new thread and run
    // No arguments provided
    bool process::run() {
        return kern->run_thread(prthr.unique_id());
    }
}
