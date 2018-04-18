#include <kernel/thread.h>
#include <core_mem.h>
#include <core_kernel.h>
#include <ptr.h>

namespace eka2l1 {
    namespace kernel {
        thread::thread(const std::string& name, const address epa,  const size_t stack_size,
                       arm::jitter_arm_type jit_type)
            : kernel_obj(name), stack_size(stack_size) {
            cpu = arm::create_jitter(jit_type);
            cpu->set_entry_point(epa);

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

            // Add the thread to the kernel management unit
            core_kernel::add_thread(this);
        }
    }
}
