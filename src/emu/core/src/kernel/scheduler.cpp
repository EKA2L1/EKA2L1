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
            //crr_running_thread = thr_real;
        }

		void thread_scheduler::switch_context(kernel::thread* oldt, kernel::thread* newt) {
			if (oldt) {
				oldt->lrt = system->get_ticks();
				jitter->save_context(oldt->ctx);

				// If it's still in run
				if (oldt->state == thread_state::run) {
					ready_threads.emplace(oldt);
					oldt->state = thread_state::ready;
				}
			}

			if (newt) {
				// cancel wake up
				system->unschedule_event(wakeup_evt, newt->obj_id);

				crr_thread = newt;
				crr_thread->state = thread_state::run;
				// TODO: remove the new thread from queue ?

				jitter->load_context(crr_thread->ctx);
			}
			else {
				// Nope
				crr_thread = nullptr;
			}
		}

		kernel::thread* thread_scheduler::next_ready_thread() {
			kernel::thread* crr = current_thread();

			if (crr && crr->current_state == thread_state::run) {
				if (ready_threads.top()->current_priority() < crr->current_priority()) {
					return crr;
				}

				return nullptr;
			}

			auto next = ready_threads.top();
			ready_threads.pop();

			return next;
		}

		void thread_scheduler::reschedule() {
			kernel::thread* crr_thread = current_thread();
			kernel::thread* next_thread = next_ready_thread();

			switch_context(crr_thread, next_thread);
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
