#pragma once

#include <condition_variable>
#include <memory>
#include <string>

#include <kernel/kernel_obj.h>
#include <common/resource.h>
#include <arm/jit_factory.h>
#include <ptr.h>

namespace eka2l1 {
    namespace kernel {
        using address = uint32_t;
        using thread_stack = common::resource<address>;
        using thread_stack_ptr = std::unique_ptr<thread_stack>;

        enum class thread_state {
            run,
            wait,
            ready,
            stop
        };

        enum thread_priority
        {
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

        class thread: public kernel_obj {
            friend class thread_scheduler;

            thread_state              state;
            arm::jitter               cpu;
            std::mutex                mut;
            std::condition_variable todo;

            // Thread context to save when suspend the execution
            arm::jit_interface::thread_context ctx;

            int priority;
            thread_stack_ptr stack;

            size_t stack_size;
            size_t min_heap_size, max_heap_size;

            size_t heap_addr;
            void* usrdata;

        protected:
            // Run without notice the order
            void run_ignore();
            void stop_ignore();

        public:
            thread();
            thread(const std::string& name, const address epa, const size_t stack_size,
                   const size_t min_heap_size, const size_t max_heap_size,
                   void* usrdata = nullptr,
                   thread_priority pri = priority_normal,
                   arm::jitter_arm_type jit_type = arm::unicorn);

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
            bool suspend();

            // Physically we can't compare thread.
            bool operator > (const thread& rhs);
            bool operator < (const thread& rhs);
            bool operator == (const thread& rhs);
            bool operator >= (const thread& rhs);
            bool operator <= (const thread& rhs);
        };

        using thread_ptr = std::shared_ptr<kernel::thread>;
    }
}
