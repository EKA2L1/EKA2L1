#include <common/log.h>
#include <core/core_timing.h>

#include <algorithm>
#include <mutex>
#include <thread>
#include <vector>

namespace eka2l1 {
    namespace core_timing {
        int slice_len;
        int downcount;

        enum {
            MAX_SLICE_LENGTH = 20000
        };

        std::vector<mhz_change_callback> internal_mhzcs;

        uint64_t global_timer;
        uint64_t idle_ticks;
        uint64_t last_global_time_ticks;
        uint64_t last_global_time_us;

        int64_t CPU_HZ;

        std::mutex mut;

        struct event_type {
            timed_callback callback;
            std::string name;
        };

        struct event {
            int event_type;
            uint64_t event_time;
            uint64_t event_user_data;
        };

        std::vector<event_type> event_types;
        std::vector<event> events, ts_events;

        void fire_mhz_changes() {
            for (auto &mhz_change : internal_mhzcs) {
                mhz_change();
            }
        }

        void set_clock_frequency_mhz(int cpu_mhz) {
            last_global_time_ticks = get_ticks();
            last_global_time_us = get_global_time_us();

            CPU_HZ = cpu_mhz;

            fire_mhz_changes();
        }

        uint32_t get_clock_frequency_mhz() {
            return CPU_HZ / 1000000;
        }

        uint64_t get_ticks() {
            return global_timer + slice_len - downcount;
        }

        uint64_t get_idle_ticks() {
            return idle_ticks;
        }

        uint64_t get_global_time_us() {
            uint64_t passed = global_timer - last_global_time_ticks;
            auto frequency = get_clock_frequency_mhz();

            return last_global_time_us + passed / frequency;
        }

        int register_event(const std::string &name, timed_callback callback) {
            event_type evtype;

            evtype.name = name;
            evtype.callback = callback;

            event_types.push_back(evtype);

            return event_types.size() - 1;
        }

        int get_downcount() {
            return downcount;
        }

        void init() {
            downcount = MAX_SLICE_LENGTH;
            slice_len = MAX_SLICE_LENGTH;
            internal_mhzcs.clear();

            global_timer = 0;
            last_global_time_ticks = 0;
            last_global_time_us = 0;
            idle_ticks = 0;
        }

        void restore_register_event(int evt_type, const std::string &name, timed_callback callback) {
            event_type evtype;

            evtype.callback = callback;
            evtype.name = name;

            event_types[evt_type] = evtype;
        }

        void unregister_all_events() {
            event_types.clear();
        }

        void add_ticks(uint32_t ticks) {
            downcount -= ticks;
        }

        void schedule_event(int64_t cycles_into_future, int event_type, uint64_t userdata) {
            std::lock_guard<std::mutex> guard(mut);
            event evt;

            evt.event_time = get_ticks() + cycles_into_future;
            evt.event_type = event_type;
            evt.event_user_data = userdata;

            events.push_back(evt);
        }

        void schedule_event_imm(int event_type, uint64_t userdata) {
            schedule_event(0, event_type, userdata);
        }

        void unschedule_event(int event_type, uint64_t usrdata) {
            std::lock_guard<std::mutex> guard(mut);

            auto res = std::find_if(events.begin(), events.end(),
                [&](auto evt) { return (evt.event_type == event_type) && (evt.event_user_data == usrdata); });

            if (res != events.end()) {
                events.erase(res);
            }
        }

        void remove_event(int event_type) {
            std::lock_guard<std::mutex> guard(mut);

            auto res = std::find_if(events.begin(), events.end(),
                [&](auto evt) { return (evt.event_type == event_type); });

            if (res != events.end()) {
                events.erase(res);
            }
        }

        void idle(int max_idle) {
            auto dc = downcount;

            if (max_idle != 0 && dc > max_idle) {
                dc = max_idle;
            }

            if (events.size() > 0 && dc > 0) {
                event first_event = events[0];

                int cexecuted = slice_len - downcount;
                int cnextevt = (int)(first_event.event_time - global_timer);

                if (cnextevt < cexecuted + dc) {
                    dc = cnextevt - cexecuted;

                    if (dc < 0) {
                        dc = 0;
                    }
                }
            }

            idle_ticks += dc;
            downcount -= dc;
        }

        void remove_all_events(int event_type) {
            remove_event(event_type);
        }

        void advance() {
            move_events();

            int cycles_executed = slice_len - downcount;
            global_timer += cycles_executed;
            slice_len = MAX_SLICE_LENGTH;

            while (!events.empty() && events.front().event_time <= global_timer) {
                event evt = std::move(events.front());
                events.pop_back();
                event_types[evt.event_type].callback(evt.event_user_data, global_timer - evt.event_time);
            }

            if (!events.empty()) {
                slice_len = std::min((int)(events.front().event_time - global_timer), (int)MAX_SLICE_LENGTH);
            }

            downcount = slice_len;
        }

        void move_events() {
            std::lock_guard<std::mutex> guard(mut);

            for (uint32_t i = 0; i < ts_events.size(); i++) {
                events.push_back(ts_events[i]);
                ts_events.erase(events.begin() + i);
            }
        }

        void shutdown() {
            move_events();
            clear_pending_events();
            unregister_all_events();
        }

        void clear_pending_events() {
            events.clear();
        }

        void log_pending_events() {
            for (auto evt : events) {
                LOG_INFO("Pending event: Time: {}, Event type pos: {}, Event userdata: {}",
                    evt.event_time, evt.event_type, evt.event_user_data);
            }
        }

        void register_mhz_change_callback(mhz_change_callback change_callback) {
            std::lock_guard<std::mutex> guard(mut);
            internal_mhzcs.push_back(change_callback);
        }

        void force_check() {
            int cycles_executed = slice_len - downcount;
            global_timer += cycles_executed;
            downcount = -1;
            slice_len = -1;
        }
    }
}
