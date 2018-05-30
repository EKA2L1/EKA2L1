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

		void thread::reset_thread_ctx(uint32_t entry_point, uint32_t stack_top) {
			ctx.pc = entry_point;
			ctx.sp = stack_top;
			ctx.cpsr = 16 | ((entry_point & 1) << 5);

			std::fill(ctx.cpu_registers.begin(), ctx.cpu_registers.end(), 0);
			std::fill(ctx.fpu_registers.begin(), ctx.fpu_registers.end(), 0);
		}

        thread::thread(kernel_system* kern, memory_system* mem, uint32_t owner, const std::string &name, const address epa, const size_t stack_size,
            const size_t min_heap_size, const size_t max_heap_size,
            void *usrdata,
            thread_priority pri)
            : wait_obj(kern, name)
            , stack_size(stack_size)
            , min_heap_size(min_heap_size)
            , max_heap_size(max_heap_size)
            , usrdata(usrdata)
            , mem(mem)
			, owner(owner) {
		     priority = caculate_thread_priority(pri);

            const thread_stack::deleter stack_deleter = [mem](address stack) {
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

			reset_thread_ctx(epa, stack_top);

			scheduler = kern->get_thread_scheduler();

            // Add the thread to the kernel management unit
            kern->add_thread(this);
        }

		bool thread::sleep(int64_t ns) {
			state = thread_state::wait;
			return scheduler->sleep(this, ns);
		}

		bool thread::run() {
			state = thread_state::run;
			kern->run_thread(obj_id);

			return true;
		}

		bool thread::stop() {
			scheduler->unschedule_wakeup();

			if (state == thread_state::ready) {
				scheduler->unschedule(obj_id);
			}

			state = thread_state::stop;

			wake_up_waiting_threads();

			for (auto& thr : waits) {
				thr->erase_waiting_thread(thr->unique_id());
			}

			waits.clear();

			// release mutex

			return true;
		}

		bool thread::resume() {
			return scheduler->resume(obj_id);
		}

		bool thread::should_wait(const kernel::uid id) {
			return state != thread_state::stop;
		}

		void thread::acquire(const kernel::uid id) {
			// :)
		}
    }
}
