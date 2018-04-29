#pragma once

#include <vector>
#include <queue>
#include <mutex>
#include <map>

namespace eka2l1 {
    namespace kernel {
        class thread;
        enum class thread_state;

        using uid = uint64_t;

        // Schedule nanothreads based on Core Timing
        class thread_scheduler {
            std::map<uid, kernel::thread*> waiting_threads;
            std::priority_queue<kernel::thread*> ready_threads;
            std::map<uid, kernel::thread*> running_threads;

            kernel::thread* crr_running_thread;
            uint32_t ticks_yield;

            int wakeup_evt;
            int yield_evt;

            std::mutex mut;

        protected:
            void yield_thread();
            void wake_thread(uint64_t id);


        public:
            // The thread scheduler register and schedule an event to the timing,
            // that each ticks_yield, call the next thread that follow the priority
            // rules
            // The constructor also register all the needed event
            thread_scheduler(uint32_t ticks_yield);

            bool schedule(kernel::thread* thread);
            bool sleep(kernel::thread* thread, uint32_t sl_time);

            kernel::thread* current_running_thread() const {
                return crr_running_thread;
            }
        };
    }
}
