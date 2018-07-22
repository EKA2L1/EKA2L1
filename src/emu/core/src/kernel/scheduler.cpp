/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <algorithm>
#include <common/log.h>
#include <core/core_kernel.h>
#include <core/core_mem.h>
#include <core/core_timing.h>
#include <core/kernel/scheduler.h>
#include <core/kernel/thread.h>
#include <functional>

void wake_thread(uint64_t ud, int cycles_late);

void wake_thread(uint64_t ud, int cycles_late) {
    eka2l1::kernel::thread *thr = reinterpret_cast<decltype(thr)>(ud);

    if (thr == nullptr) {
        return;
    }

    if (thr->current_state() == eka2l1::kernel::thread_state::wait_fast_sema
        || thr->current_state() == eka2l1::kernel::thread_state::wait_hle) {
        for (auto &wthr : thr->waits) {
            wthr->erase_waiting_thread(wthr);
        }

        thr->waits.clear();
    }
}

namespace eka2l1 {
    namespace kernel {
        thread_scheduler::thread_scheduler(kernel_system *kern, timing_system *timing, arm::jit_interface &jit)
            : kern(kern)
            , timing(timing)
            , jitter(&jit)
            , crr_thread(nullptr)
            , crr_process(nullptr) {
            wakeup_evt = timing->get_register_event("SchedulerWakeUpThread");

            if (wakeup_evt == -1) {
                wakeup_evt = timing->register_event("SchedulerWakeUpThread",
                    &wake_thread);
            }
        }

        void thread_scheduler::switch_context(thread_ptr oldt, thread_ptr newt) {
            if (oldt) {
                oldt->lrt = timing->get_ticks();
                jitter->save_context(oldt->ctx);

                // If it's still in run
                if (oldt->state == thread_state::run) {
                    ready_threads.emplace(oldt);
                    oldt->state = thread_state::ready;
                }
            }

            if (newt) {
                // cancel wake up
                timing->unschedule_event(wakeup_evt, reinterpret_cast<uint64_t>(newt.get()));

                crr_thread = newt;
                crr_thread->state = thread_state::run;

                if (!oldt || oldt->owning_process() != newt->owning_process()) {
                    crr_process = newt->owning_process();

                    memory_system *mem = kern->get_memory_system();
                    mem->set_current_page_table(crr_process->get_page_table());
                }

                running_threads.push_back(crr_thread);
                jitter->load_context(crr_thread->ctx);
            } else {
                // Nope
                crr_thread = nullptr;
            }
        }

        thread_ptr thread_scheduler::next_ready_thread() {
            thread_ptr crr = current_thread();

            if (crr && crr->current_state() == thread_state::run) {
                if (ready_threads.top()->current_priority() < crr->current_priority()) {
                    return crr;
                }

                return nullptr;
            }

            if (ready_threads.size() == 0) {
                return nullptr;
            }

            auto next = ready_threads.top();
            ready_threads.pop();

            return next;
        }

        void thread_scheduler::reschedule() {
            thread_ptr crr_thread = current_thread();
            thread_ptr next_thread = next_ready_thread();

            switch_context(crr_thread, next_thread);
        }

        // Put the thread into the ready queue to run in the next core timing yeid
        bool thread_scheduler::schedule(thread_ptr thr) {
            if (thr->state == thread_state::run || thr->state == thread_state::ready) {
                return false;
            }

            thr->state = thread_state::ready;
            ready_threads.push(thr);

            return true;
        }

        bool thread_scheduler::sleep(thread_ptr thr, uint32_t sl_time) {
            auto res = std::find(running_threads.begin(), running_threads.end(), thr);

            if (res == running_threads.end()) {
                return false;
            }

            // It's already waiting
            if (thr->state == thread_state::wait || thr->state == thread_state::ready) {
                return false;
            }

            running_threads.erase(res);
            thr->state = thread_state::wait;

            waiting_threads.push_back(thr);

            // Schedule the thread to be waken up
            timing->schedule_event(sl_time, wakeup_evt, reinterpret_cast<uint64_t>(thr.get()));

            return true;
        }

        void thread_scheduler::unschedule_wakeup() {
            timing->unschedule_event(wakeup_evt, 0);
        }

        bool thread_scheduler::wait_sema(thread_ptr thr) {
            auto res = std::find(running_threads.begin(), running_threads.end(), thr);

            if (res == running_threads.end()) {
                return false;
            }

            // It's already waiting
            if (thr->state == thread_state::wait || thr->state == thread_state::ready
                || thr->state == thread_state::wait_fast_sema) {
                return false;
            }

            running_threads.erase(res);
            thr->state = thread_state::wait_fast_sema;

            waiting_threads.push_back(thr);

            kern->prepare_reschedule();

            return true;
        }

        bool thread_scheduler::resume(thread_ptr thr) {
            auto res = std::find(waiting_threads.begin(), waiting_threads.end(), thr);

            if (res == waiting_threads.end()) {
                // Thread is not in wait
                return false;
            }

            switch (thr->state) {
            case thread_state::wait:
            case thread_state::wait_fast_sema:
            case thread_state::wait_hle:
                break;

            case thread_state::ready:
            case thread_state::run:
            case thread_state::stop:
                return false;
            }

            thr->state = thread_state::ready;

            waiting_threads.erase(res);
            ready_threads.push(thr);

            kern->prepare_reschedule();

            return true;
        }

        void thread_scheduler::unschedule(thread_ptr thr) {
            auto res = std::find(ready_threads.begin(), ready_threads.end(), thr);

            if (res == ready_threads.end()) {
                // Thread is not in ready
                return;
            }

            ready_threads.remove(thr);
        }

        void thread_scheduler::refresh() {
            ready_threads.resort();
            reschedule();
        }
    }
}
