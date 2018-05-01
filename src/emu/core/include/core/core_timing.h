#pragma once

#include <cstdint>
#include <functional>

namespace eka2l1 {
    // Based on Dolphin
    namespace core_timing {
        extern int64_t CPU_HZ;

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

        using mhz_change_callback = std::function<void()>;
        using timed_callback = std::function<void(uint64_t, int)>;

        void init();
        void shutdown();

        uint64_t get_ticks();
        uint64_t get_idle_ticks();
        uint64_t get_global_time_us();

        int register_event(const std::string &name, timed_callback callback);
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

        extern int slice_len;
    }
}
