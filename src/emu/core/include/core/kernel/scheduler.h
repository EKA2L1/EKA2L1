#pragma once

#include <arm/jit_factory.h>

#include <map>
#include <mutex>
#include <queue>
#include <vector>

namespace eka2l1 {
    class timing_system;

    namespace kernel {
        class thread;
        enum class thread_state;

        using uid = uint64_t;

        // Schedule nanothreads based on Core Timing
        class thread_scheduler {
            std::map<uid, kernel::thread *> waiting_threads;
            std::priority_queue<kernel::thread *> ready_threads;
            std::map<uid, kernel::thread *> running_threads;

            kernel::thread *crr_thread;
            uint32_t ticks_yield;

            arm::jit_interface *jitter;

            int wakeup_evt;
            int yield_evt;

            std::mutex mut;
            timing_system *system;

        protected:
            kernel::thread *next_ready_thread();

            void switch_context(kernel::thread *oldt, kernel::thread *newt);

        public:
            // The constructor also register all the needed event
            thread_scheduler(timing_system *sys, arm::jit_interface &jitter);

            void reschedule();
            void unschedule_wakeup();

            bool schedule(kernel::thread *thread);
            bool sleep(kernel::thread *thread, uint32_t sl_time);

            bool resume(kernel::uid id);
            void unschedule(kernel::uid id);

            kernel::thread *current_thread() const {
                return crr_thread;
            }
        };
    }
}
