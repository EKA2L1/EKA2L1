#include <core_kernel.h>
#include <process.h>

namespace eka2l1 {
    process::process(kernel_system* kern, memory* mem, uint32_t uid,
        const std::string &process_name, uint32_t epa, size_t min_heap_size,
        size_t max_heap_size, size_t stack_size)
        : uid(uid)
        , process_name(process_name)
        , prthr(kern, mem, process_name, epa, stack_size, min_heap_size, max_heap_size,
              nullptr, kernel::priority_normal)
        , kern(kern)
        , mem(mem) {
    }

    // Create a new thread and run
    // No arguments provided
    bool process::run() {
        return kern->run_thread(prthr.unique_id());
    }
}
