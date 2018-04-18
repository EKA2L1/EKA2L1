#pragma once

#include <condition_variable>
#include <memory>
#include <string>

#include <kernel/kernel_obj.h>
#include <common/resource.h>

namespace eka2l1 {
    namespace arm {
        class jit_interface;
        using jitter = std::unique_ptr<jit_interface>;
    }

    namespace kernel {
        using address = uint32_t;
        using thread_stack = common::resource<address>;
        using thread_stack_ptr = std::unique_ptr<thread_stack>;

        enum class thread_state {
            run,
            wait,
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
            thread_state              state;
            arm::jitter               cpu;
            std::mutex                mut;
            std::conditional_variable todo;

            // Threads are stored in a priority queue. Thread with
            // higher priority will be taken first and run.
            thread_priority priority;
            thread_stack_ptr stack;
        public:
            thread(const address epa);

            // Physically we can't compare thread.
            bool operator > (const thread& rhs);
            bool operator < (const thread& rhs);
            bool operator == (const thread& rhs);
            bool operator >= (const thread& rhs);
            bool operator <= (const thread& rhs);
        };
    }
}
