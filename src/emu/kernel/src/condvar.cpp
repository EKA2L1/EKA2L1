/*
 * Copyright (c) 2022 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <kernel/condvar.h>
#include <kernel/scheduler.h>
#include <kernel/kernel.h>
#include <kernel/mutex.h>
#include <kernel/timing.h>
#include <common/log.h>
#include <functional>

#include <utils/err.h>

namespace eka2l1::kernel {
    // Is it too simple???
    condvar::condvar(kernel_system *kern, ntimer *timing, kernel::process *owner, std::string name, kernel::access_type access)
        : kernel_obj(kern, name, owner, access)
        , timing_(timing)
        , holding_(nullptr)
        , timeout_event_(-1) {
        obj_type = object_type::condvar;

        timeout_event_ = timing_->register_event(fmt::format("CondVar{}TimeoutEvt", unique_id()),
            std::bind(&condvar::on_timeout, this, std::placeholders::_1, std::placeholders::_2));
    }

    condvar::~condvar() {
        while (!waits.empty()) {
            thread *thr = waits.top();
            waits.pop();

            thr->get_scheduler()->dewait(thr);
        }

        while (!suspended.empty()) {
            thread *thr = E_LOFF(suspended.first()->deque(), thread, suspend_link);
            thr->get_scheduler()->dewait(thr);
        }

        for (thread *timing_thr: timing_out_thrs_) {
            timing_thr->decrease_access_count();
        }

        timing_->remove_event(timeout_event_);
    }

    bool condvar::wait(thread *thr, mutex *mut, const int timeout) {
        // The thread should be in ready state
        if (thr->current_state() != kernel::thread_state::run) {
            LOG_ERROR(KERNEL, "Calling condvar wait in an abnormal state (not running!");
            return false;
        }
        if (holding_ && (holding_ != mut)) {
            LOG_ERROR(KERNEL, "Calling wait on an already waited condvar with different mutex!");
            return false;
        }
        if (!holding_) {
            holding_ = mut;
        }

        // Release the mutex
        mut->signal(mut->holder());
        thr->get_scheduler()->wait(thr);

        thr->state = thread_state::wait_condvar;
        thr->wait_obj = this;

        if (timeout > 0) {
            timing_out_thrs_.insert(thr);
            thr->increase_access_count();

            timing_->schedule_event(timeout, timeout_event_, thr->unique_id());
        }

        waits.emplace(thr);
        return true;
    }

    void condvar::on_timeout(std::uint64_t userdata, int late) {
        kernel::uid thr_id = static_cast<std::uint64_t>(userdata);
        kernel_system *kern = get_kernel_object_owner();

        kern->lock();
        thread *thr = kern->get_by_id<thread>(thr_id);

        if (!thr) {
            LOG_ERROR(KERNEL, "Thread with id {} hit timeout on condvar, but disappear magically!", thr_id);
            kern->unlock();

            return;
        }

        if (thr->state == thread_state::wait_condvar_suspend) {
            thr->suspend_link.deque();
        } else {
            waits.remove(thr);
        }

        // Set the error code (register 0)
        thr->ctx.cpu_registers[0] = epoc::error_timed_out;
        unblock_thread(thr);

        timing_out_thrs_.erase(thr);
        kern->unlock();
    }

    void condvar::unblock_thread(thread *thr) {
        thr->wait_obj = nullptr;
        timing_->unschedule_event(timeout_event_, thr->unique_id());

        if (thr->state == thread_state::wait_condvar_suspend) {
            // You will still be in waiting state
            // NOTE: Right now it's put into mutex's suspend queue waiting for resume
            // It's not reclaim right away. I think this is correct though.
            holding_->transfer_suspend_from_condvar(thr);
        } else {
            thr->get_scheduler()->dewait(thr);
            holding_->wait(thr);
        }
    }

    void condvar::signal() {
        thread *thr = nullptr;
        if (!waits.empty()) {
            thr = waits.top();
            waits.pop();
        } else if (!suspended.empty()) {
            thr = E_LOFF(suspended.first()->deque(), thread, suspend_link);      
        }

        if (thr) {
            unblock_thread(thr);
        } else {
            holding_ = nullptr;
        }
    }

    void condvar::broadcast() {
        while (!waits.empty()) {
            signal();
        }
    }

    bool condvar::suspend_thread(thread *thr) {
        waits.remove(thr);
        suspended.push(&thr->suspend_link);

        thr->state = thread_state::wait_condvar_suspend;
        return true;
    }

    bool condvar::unsuspend_thread(thread *thr) {
        thr->suspend_link.deque();
        waits.push(thr);

        thr->state = thread_state::wait_condvar;
        return true;
    }

    void condvar::priority_change() {
        waits.resort();
    }
}