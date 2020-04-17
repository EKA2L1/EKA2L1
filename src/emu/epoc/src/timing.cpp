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

#include <common/log.h>
#include <epoc/timing.h>

#include <common/chunkyseri.h>

#include <algorithm>
#include <mutex>
#include <thread>
#include <vector>

namespace eka2l1 {
    void timing_system::fire_mhz_changes() {
        for (auto &mhz_change : internal_mhzcs) {
            mhz_change();
        }
    }

    void timing_system::set_clock_frequency_mhz(int cpu_mhz) {
        last_global_time_ticks = get_ticks();
        last_global_time_us = get_global_time_us();

        CPU_HZ = cpu_mhz;

        fire_mhz_changes();
    }

    std::uint32_t timing_system::get_clock_frequency_mhz() {
        return static_cast<std::uint32_t>(CPU_HZ / 1000000);
    }

    std::uint64_t timing_system::get_ticks() {
        return global_timer + slice_len - downcount;
    }

    std::uint64_t timing_system::get_idle_ticks() {
        return idle_ticks;
    }

    uint64_t timing_system::get_global_time_us() {
        uint64_t passed = global_timer - last_global_time_ticks;
        auto frequency = get_clock_frequency_mhz();

        return last_global_time_us + passed / frequency;
    }

    int timing_system::register_event(const std::string &name, timed_callback callback) {
        event_type evtype;

        evtype.name = name;
        evtype.callback = callback;

        event_types.push_back(evtype);

        return static_cast<int>(event_types.size() - 1);
    }

    int timing_system::get_register_event(const std::string &name) {
        for (uint32_t i = 0; i < event_types.size(); i++) {
            if (event_types[i].name == name) {
                return i;
            }
        }

        return -1;
    }

    std::int64_t timing_system::get_downcount() {
        return downcount;
    }

    void timing_system::init() {
        downcount = INITIAL_SLICE_LENGTH;
        slice_len = INITIAL_SLICE_LENGTH;
        internal_mhzcs.clear();

        global_timer = 0;
        last_global_time_ticks = 0;
        last_global_time_us = 0;
        idle_ticks = 0;

        CPU_HZ = 484000000;
    }

    void timing_system::restore_register_event(int evt_type, const std::string &name, timed_callback callback) {
        event_type evtype;

        evtype.callback = callback;
        evtype.name = name;

        event_types[evt_type] = evtype;
    }

    void timing_system::swap_userdata_event(int event_type, std::uint64_t old_userdata, std::uint64_t new_userdata) {
        auto e = std::find_if(events.begin(), events.end(), [=](const event &ei) {
            return (ei.event_user_data == old_userdata) && (ei.event_type == event_type);
        });

        if (e != events.end()) {
            e->event_user_data = new_userdata;
        }
    }

    void timing_system::unregister_all_events() {
        event_types.clear();
    }

    void timing_system::add_ticks(uint32_t ticks) {
        downcount -= ticks;
    }

    void timing_system::schedule_event(int64_t cycles_into_future, int event_type, uint64_t userdata) {
        std::lock_guard<std::mutex> guard(mut);
        event evt;

        evt.event_time = get_ticks() + cycles_into_future;
        evt.event_type = event_type;
        evt.event_user_data = userdata;

        events.push_back(evt);

        std::stable_sort(events.begin(), events.end(),
            [](const event &lhs, const event &rhs) {
                return lhs.event_time > rhs.event_time;
            });
    }

    void timing_system::schedule_event_imm(int event_type, uint64_t userdata) {
        schedule_event(0, event_type, userdata);
    }

    void timing_system::unschedule_event(int event_type, uint64_t usrdata) {
        std::lock_guard<std::mutex> guard(mut);

        auto res = std::find_if(events.begin(), events.end(),
            [&](auto evt) { return (evt.event_type == event_type) && (evt.event_user_data == usrdata); });

        if (res != events.end()) {
            events.erase(res);
        }
    }

    void timing_system::remove_event(int event_type) {
        std::lock_guard<std::mutex> guard(mut);

        auto res = std::find_if(events.begin(), events.end(),
            [&](auto evt) { return (evt.event_type == event_type); });

        if (res != events.end()) {
            events.erase(res);
        }
    }

    void timing_system::idle(int max_idle) {
        auto dc = downcount;

        if (max_idle != 0 && dc > max_idle) {
            dc = max_idle;
        }

        if (events.size() > 0 && dc > 0) {
            event first_event = events[0];

            std::size_t cexecuted = slice_len - downcount;
            std::size_t cnextevt = first_event.event_time - global_timer;

            if (cnextevt < cexecuted + dc) {
                dc = static_cast<int>(cnextevt - cexecuted);

                if (dc < 0) {
                    dc = 0;
                }
            }
        }

        idle_ticks += dc;
        downcount -= dc;
    }

    void timing_system::remove_all_events(int event_type) {
        remove_event(event_type);
    }

    void timing_system::advance() {
        move_events();

        auto org_slice = slice_len;
        auto org_timer = global_timer;

        const std::int64_t cycles_executed = slice_len - downcount;
        global_timer += cycles_executed;
        slice_len = INITIAL_SLICE_LENGTH;

        while (!events.empty() && events.back().event_time <= global_timer) {
            event evt = std::move(events.back());
            events.pop_back();

            std::stable_sort(events.begin(), events.end(),
                [](const event &lhs, const event &rhs) {
                    return lhs.event_time > rhs.event_time;
                });

            event_types[evt.event_type]
                .callback(evt.event_user_data, static_cast<int>(global_timer - evt.event_time));
        }

        if (!events.empty()) {
            slice_len = std::min(static_cast<std::int64_t>(events.back().event_time - global_timer),
                static_cast<std::int64_t>(MAX_SLICE_LENGTH));
        }

        downcount = slice_len;
    }

    void timing_system::move_events() {
        std::lock_guard<std::mutex> guard(mut);

        for (uint32_t i = 0; i < ts_events.size(); i++) {
            events.push_back(ts_events[i]);

            std::stable_sort(events.begin(), events.end(),
                [](const event &lhs, const event &rhs) {
                    return lhs.event_time > rhs.event_time;
                });

            ts_events.erase(events.begin() + i);
        }
    }

    void timing_system::shutdown() {
        move_events();
        clear_pending_events();
        unregister_all_events();
    }

    void timing_system::clear_pending_events() {
        events.clear();
    }

    void timing_system::log_pending_events() {
        for (auto evt : events) {
            LOG_INFO("Pending event: Time: {}, Event type pos: {}, Event userdata: {}",
                evt.event_time, evt.event_type, evt.event_user_data);
        }
    }

    void timing_system::register_mhz_change_callback(mhz_change_callback change_callback) {
        std::lock_guard<std::mutex> guard(mut);
        internal_mhzcs.push_back(change_callback);
    }

    void timing_system::force_check() {
        const std::int64_t cycles_executed = slice_len - downcount;
        global_timer += cycles_executed;
        downcount = -1;
        slice_len = -1;
    }

    static void anticrash_callback(std::uint64_t usdata, int cycles_late) {
        LOG_ERROR("Snapshot broken: Unregister event called!");
    }

    static void event_do_state(common::chunkyseri &seri, event &evt) {
        seri.absorb(evt.event_type);
        seri.absorb(evt.event_time);
        seri.absorb(evt.event_user_data);
    }

    void timing_system::do_state(common::chunkyseri &seri) {
        std::lock_guard<std::mutex> guard(mut);
        auto s = seri.section("CoreTiming", 1);

        if (!s) {
            return;
        }

        seri.absorb(CPU_HZ);
        seri.absorb(slice_len);
        seri.absorb(global_timer);
        seri.absorb(idle_ticks);

        seri.absorb(last_global_time_ticks);
        seri.absorb(last_global_time_us);

        std::uint32_t total_event_type = static_cast<std::uint32_t>(event_types.size());
        seri.absorb(total_event_type);

        event_types.resize(total_event_type, event_type{ anticrash_callback, "INVALID" });

        // Since many events using a native pointer, storing the old userdata
        // Than the object will restore with new userdata, using swap_event_userdata.
        seri.absorb_container(events, event_do_state);

        fire_mhz_changes();
    }
}
