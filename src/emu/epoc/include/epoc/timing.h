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

    class timing_system;

    /**
     * \brief Nanokernel timer, used to track timing for a core.
     */
    class ntimer {
    private:
        std::uint32_t core_;
        std::vector<event> events_;
        eka2l1::threadsafe_cn_queue<event> ts_events_;

        timing_system *timing_;

        std::int64_t slice_len_;
        std::int64_t downcount_;

        std::uint64_t ticks_;
        std::uint64_t idle_ticks_;

        bool timer_sane_;

    protected:
        void move_events();

    public:
        explicit ntimer(timing_system *timing, const std::uint32_t core_num);

        const std::uint32_t core_number() const;
        const std::int64_t get_slice_length() const;
        const std::int64_t downcount() const;

        const std::uint64_t ticks() const;
        const std::uint64_t idle_ticks() const;

        void idle(int max_idle = 0);
        void add_ticks(uint32_t ticks);
        void advance();

        void schedule_event(int64_t cycles_into_future, int event_type, std::uint64_t userdata,
            const bool thr_safe = false);

        void unschedule_event(int event_type, uint64_t userdata);
    };

    class timing_system {
        friend class ntimer;

        std::int64_t CPU_HZ;
        std::mutex mut_;

        std::uint32_t current_core_;

        std::vector<mhz_change_callback> internal_mhzcs_;
        std::vector<event_type> event_types_;

        std::vector<std::unique_ptr<ntimer>> timers_;

    protected:
        void fire_mhz_changes();

    public:
        inline int64_t ms_to_cycles(int ms) {
            return CPU_HZ / 1000 * ms;
        }

        inline int64_t ms_to_cycles(uint64_t ms) {
            return CPU_HZ / 1000 * ms;
        }

        inline int64_t ms_to_cycles(float ms) {
            return (int64_t)(CPU_HZ * ms * (0.001f));
        }

        inline int64_t ms_to_cycles(double ms) {
            return (int64_t)(CPU_HZ * ms * (0.001));
        }

        inline int64_t us_to_cycles(float us) {
            return (int64_t)(CPU_HZ * us * (0.000001f));
        }

        inline int64_t us_to_cycles(int us) {
            return (CPU_HZ / 1000000 * (int64_t)us);
        }

        inline int64_t us_to_cycles(int64_t us) {
            return (CPU_HZ / 1000000 * us);
        }

        inline int64_t us_to_cycles(uint64_t us) {
            return (int64_t)(CPU_HZ / 1000000 * us);
        }

        inline int64_t cycles_to_us(int64_t cycles) {
            return cycles / (CPU_HZ / 1000000);
        }

        inline int64_t ns_to_cycles(uint64_t us) {
            return (int64_t)(CPU_HZ / 1000000000 * us);
        }

        ntimer *current_timer();

        void init(const int total_core = 1);
        void shutdown();

        std::uint64_t get_global_time_us();
        const std::uint64_t ticks();
        const std::uint64_t idle_ticks();

        const std::int64_t downcount();

        int register_event(const std::string &name, timed_callback callback);
        int get_register_event(const std::string &name);

        void restore_register_event(int event_type, const std::string &name, timed_callback callback);
        void unregister_all_events();

        void remove_event(int event_type);
        void remove_all_events(int event_type);

        void advance();
        void add_ticks(uint32_t ticks);
        void idle(int max_idle = 0);

        void schedule_event(int64_t cycles_into_future, int event_type, std::uint64_t userdata,
            const bool thr_safe = false);

        void unschedule_event(int event_type, uint64_t userdata);

        void register_mhz_change_callback(mhz_change_callback change_callback);
        void set_clock_frequency_mhz(int cpu_mhz);

        uint32_t get_clock_frequency_mhz();
    };
}
