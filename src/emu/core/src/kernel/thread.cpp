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

        thread::thread(const std::string &name, const address epa, const size_t stack_size,
            const size_t min_heap_size, const size_t max_heap_size,
            void *usrdata,
            thread_priority pri, arm::jitter_arm_type jit_type)
            : kernel_obj(name)
            , stack_size(stack_size)
            , min_heap_size(min_heap_size)
            , max_heap_size(max_heap_size)
            , usrdata(usrdata) {
            cpu = arm::create_jitter(jit_type);
            cpu->set_entry_point(epa);

            priority = caculate_thread_priority(pri);

            const thread_stack::deleter stack_deleter = [](address stack) {
                core_mem::free(stack);
            };

            stack = std::make_unique<thread_stack>(
                core_mem::alloc(stack_size), stack_deleter);

            const address stack_top = stack->get() + stack_size;

            ptr<uint8_t> stack_phys_beg(stack->get());
            ptr<uint8_t> stack_phys_end(stack->get() + stack_size);

            // Fill the stack with garbage
            std::fill(stack_phys_beg.get(), stack_phys_end.get(), 0xcc);

            cpu->set_stack_top(stack_top);

            heap_addr = core_mem::alloc(1);

            if (!heap_addr) {
                LOG_ERROR("No more heap for thread!");
            }

            // Set the thread region: where to alloc memory
            core_mem::set_crr_thread_heap_region(heap_addr, max_heap_size);

            // Add the thread to the kernel management unit
            core_kernel::add_thread(this);
        }

        void thread::run_ignore() {
            cpu->run();
        }

        void thread::stop_ignore() {
            cpu->stop();
        }
    }
}
