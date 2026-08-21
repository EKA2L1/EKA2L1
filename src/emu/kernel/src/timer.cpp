/*
 * Copyright (c) 2018 EKA2L1 Team / Citra Team
 * 
 * This file is part of EKA2L1 project / Citra Emulator Project
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

#include <common/common.h>
#include <common/cvt.h>
#include <common/log.h>

#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/timer.h>
#include <utils/err.h>
#include <utils/reqsts.h>

namespace eka2l1 {
    namespace kernel {
        void timer_callback(kernel_system *kern, uint64_t user, int ns_late);

        // A timer event can become due while the guest is mid-way through issuing
        // the request: it has written the request status (pending) but has not yet
        // run SetActive() on the owning active object. Completing it then strands
        // the active scheduler with a request it cannot see as ready, which it
        // reports as the stray-signal panic E32USER-CBase 46. When that happens we
        // reschedule the completion a short moment later so the still-running guest
        // can finish issuing; the count is bounded so a bare TRequestStatus (waited
        // on with User::WaitForRequest, which never sets the active flag) still
        // completes after a negligible delay.
        static constexpr std::int64_t TIMER_ACTIVATE_DEFER_US = 100;
        static constexpr int TIMER_ACTIVATE_DEFER_LIMIT = 8;

        timer::timer(kernel_system *kern, ntimer *timing, std::string name,
            kernel::access_type access)
            : kernel_obj(kern, name, nullptr, access)
            , timing(timing)
            , outstanding(false) {
            obj_type = object_type::timer;

            callback_type = timing->get_register_event("TimerCallback");

            if (callback_type == -1) {
                // The event's userdata is the timer's kernel object id, not a
                // pointer: a timer can be destroyed while its callback is already
                // in flight on the timing thread (unschedule_event cannot recall
                // it), so the callback re-resolves the id under the kernel lock
                // and a dead timer becomes a safe no-op. The captured kernel
                // shares the ntimer's lifetime (both torn down with the system).
                kernel_system *kern_of_event = kern;
                callback_type = timing->register_event("TimerCallback",
                    [kern_of_event](std::uint64_t user, int ns_late) {
                        timer_callback(kern_of_event, user, ns_late);
                    });
            }
        }

        timer::~timer() {
            timing->unschedule_event(callback_type, static_cast<std::uint64_t>(unique_id()));
        }

        bool timer::after(kernel::thread *requester, eka2l1::ptr<epoc::request_status> sts, std::uint64_t us_signal) {
            if (outstanding) {
                return false;
            }

            outstanding = true;
            activate_defer_count_ = 0;

            info.done_nof = epoc::notify_info(sts, requester);
            info.own_timer = this;

            static constexpr std::uint64_t MINIMUM_US_AFTER = 30;

            // Simulate some timeslice delay, and not finish immediately
            // Some games just set the microseconds to signal to 1, and then when it's report superfast, it acts weird!
            // For example: DDragon, which signals an object that has not yet been set to active in time! (cancel was called but ineffective cause finish got to it first)
            timing->schedule_event(common::max<std::uint64_t>(MINIMUM_US_AFTER, us_signal),
                callback_type, static_cast<std::uint64_t>(unique_id()));
            return true;
        }
        
        bool timer::after_ticks(kernel::thread *requester, eka2l1::ptr<epoc::request_status> sts,
            std::uint64_t tick_count) {
            const std::uint64_t us_per_ticks = (common::microsecs_per_sec / epoc::TICK_TIMER_HZ);
            return after(requester, sts, us_per_ticks * tick_count);
        }

        bool timer::request_finish() {
            if (!outstanding) {
                return false;
            }

            outstanding = false;
            return true;
        }

        bool timer::cancel_request() {
            if (!outstanding) {
                // Do a signal so that the semaphore won't lock the thread up next time it waits
                // info.own_thread->signal_request();
                return false;
            }

            info.done_nof.complete(epoc::error_cancel);

            // If the timer hasn't finished yet, please unschedule it.
            if (outstanding) {
                // Cancel
                timing->unschedule_event(callback_type, static_cast<std::uint64_t>(unique_id()));
            }

            return request_finish();
        }

        bool timer::fire_or_defer() {
            if (!outstanding) {
                return false;
            }

            // EKA1 does not carry the active/pending request-status flags, so the
            // race below cannot be detected there and the request completes at once.
            kernel::thread *requester = info.done_nof.requester;
            if (requester && !kern->is_eka1() && (activate_defer_count_ < TIMER_ACTIVATE_DEFER_LIMIT)) {
                epoc::request_status *sts = info.done_nof.sts.get(requester->owning_process());

                // Only defer while the requester is still running (not parked on
                // its request semaphore): the race is the guest issuing the request
                // and about to call SetActive. A thread blocked in WaitForRequest on
                // a bare TRequestStatus (which never goes active) has a negative
                // count, so it completes immediately with no added latency.
                if (sts && (sts->flags & epoc::request_status::pending)
                    && !(sts->flags & epoc::request_status::active)
                    && (requester->request_count() >= 0)) {
                    // Request issued but not yet made active: let the guest run on
                    // and reschedule this completion a touch later.
                    activate_defer_count_++;
                    timing->schedule_event(TIMER_ACTIVATE_DEFER_US, callback_type,
                        static_cast<std::uint64_t>(unique_id()));
                    return false;
                }
            }

            activate_defer_count_ = 0;
            outstanding = false;
            return true;
        }

        void timer::fire() {
            if (!fire_or_defer()) {
                return;
            }

            info.done_nof.complete(epoc::error_none);
        }

        void timer_callback(kernel_system *kern, uint64_t user, int ns_late) {
            // Resolve the timer from its object id under the kernel lock: the
            // timer may have been destroyed (under that same lock) after this
            // event was popped from the queue but before we got here.
            kern->lock();

            kernel::timer *tim = kern->get_by_id<kernel::timer>(static_cast<kernel::uid>(user));
            if (tim) {
                tim->fire();
            }

            kern->unlock();
        }
    }
}
