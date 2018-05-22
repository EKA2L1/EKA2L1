#include <common/log.h>
#include <core_kernel.h>
#include <core_mem.h>
#include <kernel/thread.h>
#include <ptr.h>

namespace eka2l1 {
    namespace kernel {
        int caculate_thread_priority(thread_priority pri) {
            const uint8_t pris[] = {
                1, 1, 2, 3, 4, 5, 22, 0,
                3, 5, 6, 7, 8, 9, 22, 0,
                3, 10, 11, 12, 13, 14, 22, 0,
                3, 17, 18, 19, 20, 22, 23, 0,
                9, 15, 16, 21, 24, 25, 28, 0,
                9, 15, 16, 21, 24, 25, 28, 0,
                9, 15, 16, 21, 24, 25, 28, 0,
                18, 26, 27, 28, 29, 30, 31, 0
            };

            // The owning process, in this case is always have the priority
            // of 3 (foreground)

            int idx = (3 << 3) + (int)pri;
            return pris[idx];
        }

        thread::thread(kernel_system* kern, memory_system* mem, const std::string &name, const address epa, const size_t stack_size,
            const size_t min_heap_size, const size_t max_heap_size,
            void *usrdata,
            thread_priority pri)
            : kernel_obj(kern, name)
            , stack_size(stack_size)
            , min_heap_size(min_heap_size)
            , max_heap_size(max_heap_size)
            , usrdata(usrdata)
            , mem(mem) {
            priority = caculate_thread_priority(pri);

            const thread_stack::deleter stack_deleter = [&](address stack) {
                mem->free(stack);
            };

            stack = std::make_unique<thread_stack>(
                mem->alloc(stack_size), stack_deleter);

            const address stack_top = stack->get() + stack_size;

            ptr<uint8_t> stack_phys_beg(stack->get());
            ptr<uint8_t> stack_phys_end(stack->get() + stack_size);

            // Fill the stack with garbage
            std::fill(stack_phys_beg.get(mem), stack_phys_end.get(mem), 0xcc);

            heap_addr = mem->alloc(1);

            if (!heap_addr) {
                LOG_ERROR("No more heap for thread!");
            }

            // Create thread context

            // Add the thread to the kernel management unit
            kern->add_thread(this);
        }
    }
}
