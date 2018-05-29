#pragma once

#include <condition_variable>
#include <memory>
#include <string>

#include <arm/jit_factory.h>
#include <common/resource.h>
#include <kernel/wait_obj.h>
#include <ptr.h>

namespace eka2l1 {
    class kernel_system;
    class memory;

    namespace kernel {
        using address = uint32_t;
        using thread_stack = common::resource<address>;
        using thread_stack_ptr = std::unique_ptr<thread_stack>;

		class thread_scheduler;

        enum class thread_state {
            run,
            wait,
            ready,
            stop,
			wait_fast_sema,  // Wait for semaphore
			wait_dfc,   // Unused
			wait_hle    // Wait in case an HLE event is taken place - e.g GUI
        };

        enum thread_priority {
            priority_null = -30,
            priority_much_less = -20,
            priority_less = -10,
            priority_normal = 0,
            priority_more = 10,
            priority_much_more = 20,
            priority_real_time = 30,
            priority_absolute_very_low = 100,
            priority_absolute_low = 200,
            priority_absolute_background = 300,
            priorty_absolute_forground = 400,
            priority_absolute_high = 500
        };

        class thread : public wait_obj {
            friend class thread_scheduler;

            thread_state state;
            std::mutex mut;
            std::condition_variable todo;

            // Thread context to save when suspend the execution
            arm::jit_interface::thread_context ctx;

            int priority;

            size_t stack_size;
            size_t min_heap_size, max_heap_size;

            size_t heap_addr;
            void *usrdata;

            memory_system* mem;
			uint32_t lrt;

			thread_stack_ptr stack;

			// Owner of the thread
			uint32_t owner;
			std::shared_ptr<thread_scheduler> scheduler;  // The scheduler that schedules this thread

        public:

            thread();
            thread(kernel_system* kern, memory_system* mem, uint32_t owner, const std::string &name, const address epa, const size_t stack_size,
                const size_t min_heap_size, const size_t max_heap_size,
                void *usrdata = nullptr,
                thread_priority pri = priority_normal);

            thread_state current_state() const {
                return state;
            }

            int current_priority() const {
                return priority;
            }

            void current_state(thread_state st) {
                state = st;
            }

            bool run();
			bool stop();

			bool sleep(int64_t ns);
			bool resume();

			bool should_wait(const kernel::uid id) override;
			void acquire(const kernel::uid id) override;

            // Physically we can't compare thread.
            bool operator>(const thread &rhs);
            bool operator<(const thread &rhs);
            bool operator==(const thread &rhs);
            bool operator>=(const thread &rhs);
            bool operator<=(const thread &rhs);
        };

        using thread_ptr = std::shared_ptr<kernel::thread>;
    }
}
