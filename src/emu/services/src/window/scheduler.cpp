/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
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

#include <kernel/kernel.h>
#include <kernel/timing.h>
#include <services/window/scheduler.h>
#include <services/window/screen.h>

#include <cassert>

namespace eka2l1::epoc {
    static void on_anim_due(std::uint64_t userdata, const int cycles_late) {
        anim_due_callback_data *callback = reinterpret_cast<anim_due_callback_data *>(userdata);
        callback->sched->invoke_due_animation(callback->driver, callback->screen_number);
    }

    static void on_scan_callback(std::uint64_t userdata, const int cycles_late) {
        sched_scan_callback_data *callback = reinterpret_cast<sched_scan_callback_data *>(userdata);
        callback->sched->idle_callback(callback->driver);
    }

    animation_scheduler::animation_scheduler(kernel_system *kern, ntimer *timing, const int total_screen)
        : kern_(kern)
        , timing_(timing)
        , callback_scheduled_(false) {
        anim_due_evt_ = timing_->register_event("anim_sched_anim_due_evt", on_anim_due);
        callback_evt_ = timing->register_event("anim_sched_callback_evt", on_scan_callback);

        // Make callback data
        schedules_.resize(total_screen);
        states_.resize(total_screen);
        callback_datas_.resize(total_screen);

        for (int i = 0; i < total_screen; i++) {
            schedules_[i].scheduled = false;
            states_[i].flags = screen_state::inactive;

            callback_datas_[i].sched = this;
            callback_datas_[i].screen_number = i;
        }
    }

    void animation_scheduler::schedule(drivers::graphics_driver *driver, screen *scr, const std::uint64_t time) {
        // Get screen number
        if (schedules_.size() <= scr->number) {
            return;
        }

        // Get the schedule
        anim_schedule sched;
        sched.scr = scr;
        sched.time = time;
        sched.scheduled = true;

        if (schedules_[scr->number].scheduled) {
            // Readjust the time
            schedules_[scr->number].time = time;
        } else {
            // Replace with our new sched
            schedules_[scr->number] = sched;
        }

        schedule_scans(driver);
    }

    void animation_scheduler::unschedule(const int screen_number) {
        if (schedules_.size() <= screen_number) {
            return;
        }

        schedules_[screen_number].scheduled = false;
    }

    animation_scheduler::anim_schedule *animation_scheduler::get_scheduled_screen_update(const int scr_num) {
        if (schedules_.size() <= scr_num) {
            return nullptr;
        }

        if (!schedules_[scr_num].scheduled) {
            return nullptr;
        }

        return &(schedules_[scr_num]);
    }

    std::int64_t animation_scheduler::get_due_delta(const bool force_redraw, const std::uint64_t due) {
        if (force_redraw) {
            // use granularity
            // For now, 0
            // TODO: Split animation vs redraw grace granularity
            return 0;
        }

        // TODO: Clamp to not bigger than 30 mins maybe?
        return std::max<std::int64_t>(due - timing_->microseconds(), 0);
    }

    void animation_scheduler::scan_for_redraw(drivers::graphics_driver *driver, const int screen_number, const bool force_redraw) {
        anim_schedule *sched = get_scheduled_screen_update(screen_number);

        if (sched) {
            // Get screen state, check if it's active
            // This is for future, for now we are doing all sync
            screen_state &scr_state = states_[screen_number];

            if (scr_state.flags != screen_state::active) {
                // Get the due
                const std::int64_t until_due = get_due_delta(force_redraw, sched->time);
                bool should_update = true;

                const std::uint64_t now = timing_->microseconds();

                if (scr_state.flags == screen_state::scheduled) {
                    // The screen update has been scheduled before. In that case, we need to check the time
                    // the redraw was supposed to be trigger.
                    //
                    // If the due of this schedule is sooner, cancel the existing screen redraw schedule
                    // and replace with our due.
                    //
                    // Else, wait for the existing update to finish, then do redraw.
                    if (until_due < static_cast<std::int64_t>(scr_state.time_expected_redraw) - static_cast<std::int64_t>(now)) {
                        // Cancel the schedule
                        timing_->unschedule_event(anim_due_evt_, reinterpret_cast<std::uint64_t>(&callback_datas_[screen_number]));
                    } else {
                        should_update = false;
                    }
                }

                if (should_update) {
                    callback_datas_[screen_number].driver = driver;

                    if (until_due == 0) {
                        // Invoke the due right away.
                        scr_state.time_expected_redraw = now;
                        invoke_due_animation(driver, screen_number);
                    } else {
                        scr_state.time_expected_redraw = now + until_due;
                        scr_state.flags = screen_state::scheduled;
                        timing_->schedule_event(static_cast<int64_t>(until_due), anim_due_evt_, reinterpret_cast<std::uint64_t>(&callback_datas_[screen_number]));
                    }
                }
            }
        }
    }

    void animation_scheduler::invoke_due_animation(drivers::graphics_driver *driver, const int screen_number) {
        anim_schedule *sched = get_scheduled_screen_update(screen_number);
        assert(sched && "The returned schedule should not be nullptr");

        sched->scheduled = false;

        {
            const std::lock_guard<std::mutex> guard(sched->scr->screen_mutex);

            kern_->lock();

            // Do redraw, now!
            sched->scr->redraw(driver);

            std::uint64_t wait_time = 0;
            sched->scr->vsync(timing_, wait_time);

            kern_->unlock();
        }

        // Transtition the state to inactive.
        states_[screen_number].flags = screen_state::inactive;
    }

    void animation_scheduler::idle_callback(drivers::graphics_driver *driver) {
        callback_scheduled_ = false;

        for (int screen_num = 0; screen_num < states_.size(); screen_num++) {
            scan_for_redraw(driver, screen_num, false);
        }
    }

    void animation_scheduler::schedule_scans(drivers::graphics_driver *driver) {
        // Schedule scan, create some screen update delay to be correspond with hardware.
        // TODO: Maybe get rid of this function?
        static constexpr std::uint16_t scheduled_us = 500;

        if (!callback_scheduled_) {
            // Schedule it
            scan_callback_data_.driver = driver;
            scan_callback_data_.sched = this;

            timing_->schedule_event(scheduled_us, callback_evt_, reinterpret_cast<std::uint64_t>(&scan_callback_data_));
            callback_scheduled_ = true;
        }
    }
}