#include <algorithm>
#include <core_timing.h>
#include <functional>
#include <kernel/scheduler.h>
#include <kernel/thread.h>
#include <core_timing.h>


namespace eka2l1 {
    namespace kernel {
        thread_scheduler::thread_scheduler(timing_system* system, arm::jit_interface& jit)
            : system(system)
            , jitter(&jit)
            , crr_running_thread(nullptr) {
            wakeup_evt = system->register_event("SchedulerWakeUpThread",
                std::bind(&thread_scheduler::wake_thread, this, std::placeholders::_1));
        }

        void thread_scheduler::wake_thread(uint64_t id) {
            const std::unique_lock<std::mutex> ul(mut);
            auto thr = waiting_threads.find(id);

            if (thr == waiting_threads.end()) {
                return;
            }

            waiting_threads.erase(id);
            thread *thr_real = thr->second;

            thr_real->state = thread_state::run;
            crr_running_thread = thr_real;
        }

        // Put the thread into the ready queue to run in the next core timing yeid
        bool thread_scheduler::schedule(kernel::thread *thr) {
            if (thr->state == thread_state::run || thr->state == thread_state::ready) {
                return false;
            }

            thr->state = thread_state::ready;
            ready_threads.push(thr);

            return true;
        }

        bool thread_scheduler::sleep(kernel::thread *thread, uint32_t sl_time) {
            // It's already waiting
            if (thread->state == thread_state::wait || thread->state == thread_state::ready) {
                return false;
            }

            running_threads.erase(thread->unique_id());
            thread->state = thread_state::wait;

            waiting_threads.emplace(thread->unique_id(), thread);

            // Schedule the thread to be waken up
            system->schedule_event(sl_time, wakeup_evt, thread->unique_id());

            return true;
        }
    }
}
