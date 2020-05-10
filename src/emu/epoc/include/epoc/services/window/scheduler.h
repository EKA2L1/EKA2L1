/*
 * Copyright (c) 2019 EKA2L1 Team
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

#pragma once

#include <cstdint>
#include <vector>

namespace eka2l1 {
    class ntimer;
    class kernel_system;

    namespace drivers {
        class graphics_driver;
    }
};

namespace eka2l1::epoc {
    class animation_scheduler;
    struct screen;

    struct anim_due_callback_data {
        int screen_number;
        animation_scheduler *sched;
        drivers::graphics_driver *driver;
    };

    struct sched_scan_callback_data {
        animation_scheduler *sched;
        drivers::graphics_driver *driver;
    };

    /**
     * \brief A scheduler which schedules screen update.
     * 
     * The scheduler takes screen redraw request and schedule them. If a schedule happens on a screen
     * that already scheduled its redraw, the due time will be updated.
     * 
     * The scheduler will scans and triggers redraw on these occasions:
     * - When all screen are idled (scans triggers in granularity).
     * - Force redraw are called (happens on screen flushing: get scan line, get screenshot, etc...)
     */
    class animation_scheduler {
        struct anim_schedule {
            screen *scr;
            bool scheduled = false;
            std::uint64_t time;
        };

        struct screen_state {
            enum {
                inactive = 0,
                active = 1 << 0,
                scheduled = 1 << 1
            };

            std::uint8_t flags;
            std::uint64_t time_expected_redraw;
        };

        std::vector<anim_schedule> schedules_;
        std::vector<screen_state> states_;
        std::vector<anim_due_callback_data> callback_datas_;

        ntimer *timing_;
        kernel_system *kern_;

        sched_scan_callback_data scan_callback_data_;

        int anim_due_evt_;
        int callback_evt_;

        bool callback_scheduled_;

        void schedule_scans(drivers::graphics_driver *driver);

    public:
        explicit animation_scheduler(kernel_system *kern, ntimer *timing, const int total_screen);

        /**
         * \brief Callback for queueing screen redraw, when the scheduler is idled.
         */
        void idle_callback(drivers::graphics_driver *driver);

        /**
         * \brief Invoke when animation is triggered, and handle redraw.
         * 
         * \param screen_number The number of the screen to do redraw. Based of 0.
         */
        void invoke_due_animation(drivers::graphics_driver *driver, const int screen_number);

        /***
         * \brief Get the current schedule for this screen.
         * \param Nullptr if none.
         */
        anim_schedule *get_scheduled_screen_update(const int scr_num);

        std::int64_t get_due_delta(const bool force_redraw, const std::uint64_t due);

        void scan_for_redraw(drivers::graphics_driver *driver, const int screen_number, const bool force_redraw);

        /**
         * \brief Schedule a screen redraw.
         * \param scr    The screen to be scheduled.
         * \param time   Time for the redraw to occurs, since 1/1/1970.
         */
        void schedule(drivers::graphics_driver *driver, screen *scr, const std::uint64_t time);

        /**
         * \brief Unschedule a screen redraw.
         * \param screen_number The number of the screen.
         */
        void unschedule(const int screen_number);
    };
}