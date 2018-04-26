#include <kernel/scheduler.h>
#include <algorithm>
#include <functional>
#include <kernel/thread.h>
#include <core_timing.h>

namespace eka2l1 {
    namespace kernel {
        thread_scheduler::thread_scheduler(uint32_t ticks_yield)
            : ticks_yield(ticks_yield), crr_running_thread(nullptr) {
            // Register event for core_timing
            yield_evt = core_timing::register_event("ScheduleryieldNextThread",
                                                    std::bind(&thread_scheduler::yield_thread, this));
            wakeup_evt = core_timing::register_event("SchedulerWakeUpThread",
                                                     std::bind(&thread_scheduler::wake_thread, this, std::placeholders::_1));

            core_timing::schedule_event(ticks_yield, yield_evt);
        }

        void thread_scheduler::yield_thread() {
            // Don't do anything, or else you might break things
            if (ready_threads.empty()) {
                core_timing::schedule_event(ticks_yield, yield_evt);
                return;
            }

            auto take_thread = std::move(ready_threads.top());

            const std::unique_lock<std::mutex> ul(mut);

            ready_threads.pop();
            take_thread->current_state(thread_state::run);
            running_threads.emplace(take_thread->unique_id(), take_thread);
            crr_running_thread = take_thread;
            take_thread->run_ignore();

            core_timing::schedule_event(ticks_yield, yield_evt);
        }

        void thread_scheduler::wake_thread(uint64_t id) {
            const std::unique_lock<std::mutex> ul(mut);
            auto thr = waiting_threads.find(id);

            if (thr == waiting_threads.end()) {
                return;
            }

            waiting_threads.erase(id);
            thread* thr_real = thr->second;

            thr_real->state = thread_state::run;
            crr_running_thread = thr_real;
            thr_real->run_ignore();
        }

        // Put the thread into the ready queue to run in the next core timing yeid
        bool thread_scheduler::schedule(kernel::thread* thr) {
            if (thr->state == thread_state::run || thr->state == thread_state::ready) {
                return false;
            }

            thr->state = thread_state::ready;
            ready_threads.push(thr);

            return true;
        }

        bool thread_scheduler::sleep(kernel::thread* thread, uint32_t sl_time) {
            // It's already waiting
            if (thread->state == thread_state::wait || thread->state == thread_state::ready) {
                return false;
            }

            running_threads.erase(thread->unique_id());

            thread->state = thread_state::wait;
            thread->stop_ignore();

            waiting_threads.emplace(thread->unique_id(), thread);

            // Schedule the thread to be waken up
            core_timing::schedule_event(sl_time, wakeup_evt, thread->unique_id());

            return true;
        }
    }
}
