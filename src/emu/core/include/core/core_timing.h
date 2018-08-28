/*
 * Copyright (c) 2018 EKA2L1 Team / 2008 Dolphin Emulator Project
 * 
 * This file is part of EKA2L1 project / Dolphin Emulator Project
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
#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace eka2l1 {
    // Based on Dolphin
    using mhz_change_callback = std::function<void()>;
    using timed_callback = std::function<void(uint64_t, int)>;
    using dfc = timed_callback;

    enum {
        MAX_SLICE_LENGTH = 20000
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

    // class NTimer
    class timing_system {
        int slice_len;
        int downcount;

        int64_t CPU_HZ;

        uint64_t global_timer;
        uint64_t idle_ticks;
        uint64_t last_global_time_ticks;
        uint64_t last_global_time_us;

        std::mutex mut;

        std::vector<mhz_change_callback> internal_mhzcs;

        std::vector<event_type> event_types;
        std::vector<event> events, ts_events;

        void fire_mhz_changes();

    public:
        inline int64_t ms_to_cycles(int ms) {
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

        inline int64_t ms_to_cycles(uint64_t ms) {
            return (int64_t)(CPU_HZ / 1000000 * ms);
        }

        void init();
        void shutdown();

        uint64_t get_ticks();
        uint64_t get_idle_ticks();
        uint64_t get_global_time_us();

        int register_event(const std::string &name, timed_callback callback);
        int get_register_event(const std::string &name);

        void restore_register_event(int event_type, const std::string &name, timed_callback callback);
        void unregister_all_events();

        void schedule_event(int64_t cycles_into_future, int event_type, uint64_t userdata = 0);
        void schedule_event_imm(int event_type, uint64_t userdata = 0);
        void unschedule_event(int event_type, uint64_t userdata);

        void remove_event(int event_type);
        void remove_all_events(int event_type);
        void advance();
        void move_events();

        void add_ticks(uint32_t ticks);

        void force_check();

        void idle(int max_idle = 0);
        void clear_pending_events();
        void log_pending_events();

        void register_mhz_change_callback(mhz_change_callback change_callback);

        void set_clock_frequency_mhz(int cpu_mhz);
        uint32_t get_clock_frequency_mhz();
        int get_downcount();
    };
}

