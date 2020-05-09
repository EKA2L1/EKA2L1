/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <common/time.h>
#include <common/queue.h>

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace eka2l1 {
    using mhz_change_callback = std::function<void()>;
    using timed_callback = std::function<void(uint64_t, int)>;

    enum {
        MAX_SLICE_LENGTH = 20000,
        INITIAL_SLICE_LENGTH = 20000
    };

    struct event_type {
        timed_callback callback;
        std::string name;
    };

    struct event {
        int event_type;
        uint64_t event_time;
        uint64_t event_user_data;
    };

    namespace common {
        class chunkyseri;
    }

    class ntimer;

    /**
     * @brief Nanokernel timer.
     */
    class ntimer {
    private:
        std::vector<event> events_;
        std::mutex lock_;
        std::mutex new_event_avail_lock_;

        std::condition_variable new_event_avail_var_;

        std::unique_ptr<common::teletimer> teletimer_;
        std::unique_ptr<std::thread> timer_thread_;          ///< Timer thread to executes callbacks

        std::vector<event_type> event_types_;
        std::uint32_t CPU_HZ_;

        std::atomic<bool> should_stop_;

    protected:
        void loop();

    public:
        explicit ntimer(const std::uint32_t cpu_hz);
        ~ntimer();

        inline int64_t ms_to_cycles(int ms) {
            return CPU_HZ_ / 1000 * ms;
        }

        inline int64_t ms_to_cycles(uint64_t ms) {
            return CPU_HZ_ / 1000 * ms;
        }

        inline int64_t ms_to_cycles(float ms) {
            return (int64_t)(CPU_HZ_ * ms * (0.001f));
        }

        inline int64_t ms_to_cycles(double ms) {
            return (int64_t)(CPU_HZ_ * ms * (0.001));
        }

        inline int64_t us_to_cycles(float us) {
            return (int64_t)(CPU_HZ_ * us * (0.000001f));
        }

        inline int64_t us_to_cycles(int us) {
            return (CPU_HZ_ / 1000000 * (int64_t)us);
        }

        inline int64_t us_to_cycles(int64_t us) {
            return (CPU_HZ_ / 1000000 * us);
        }

        inline int64_t us_to_cycles(uint64_t us) {
            return (int64_t)(CPU_HZ_ / 1000000 * us);
        }

        inline int64_t cycles_to_us(int64_t cycles) {
            return cycles / (CPU_HZ_ / 1000000);
        }

        inline int64_t ns_to_cycles(uint64_t us) {
            return (int64_t)(CPU_HZ_ / 1000000000 * us);
        }

        const std::uint64_t ticks();
        const std::uint64_t nanoseconds();
        const std::uint64_t microseconds();

        /**
         * @brief       Advance the timer.
         * @returns     Nanoseconds to next timer.
         */
        std::optional<std::uint64_t> advance();

        int register_event(const std::string &name, timed_callback callback);
        int get_register_event(const std::string &name);
        void unregister_all_events();
        void remove_event(int event_type);

        void schedule_event(int64_t ns_into_future, int event_type, std::uint64_t userdata);
        bool unschedule_event(int event_type, uint64_t userdata);
        
        bool set_clock_frequency_mhz(const std::uint32_t cpu_mhz);
        std::uint32_t get_clock_frequency_mhz();
    };
}
