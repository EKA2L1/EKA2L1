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

#include <common/algorithm.h>
#include <common/chunkyseri.h>
#include <common/log.h>

#include <epoc/timing.h>

#include <algorithm>
#include <mutex>
#include <thread>
#include <vector>

namespace eka2l1 {
    ntimer::ntimer(timing_system *timing, const std::uint32_t core_num)
        : timing_(timing)
        , core_(core_num)
        , slice_len_(INITIAL_SLICE_LENGTH)
        , downcount_(INITIAL_SLICE_LENGTH)
        , timer_sane_(false) {

    }

    void ntimer::move_events() {
        while (std::optional<event> evt = ts_events_.pop()) {
            events_.push_back(evt.value());
        }

        // Sort the event
        std::stable_sort(events_.begin(), events_.end(), [](const event &lhs, const event &rhs) {
            return lhs.event_time > rhs.event_time;
        });
    }

    const std::uint32_t ntimer::core_number() const {
        return core_;
    }

    const std::int64_t ntimer::get_slice_length() const {
        return slice_len_;
    }
    
    const std::int64_t ntimer::downcount() const {
        return downcount_;
    }

    const std::uint64_t ntimer::ticks() const {
        if (timer_sane_) {
            return ticks_ + slice_len_ - downcount_;
        }

        return ticks_;
    }

    const std::uint64_t ntimer::idle_ticks() const {
        return idle_ticks_;
    }

    void ntimer::idle(int max_idle) {
        auto dc = downcount_;

        if (max_idle != 0 && dc > max_idle) {
            dc = max_idle;
        }

        if (events_.size() > 0 && dc > 0) {
            event first_event = events_[0];

            std::size_t cexecuted = slice_len_ - downcount_;
            std::size_t cnextevt = first_event.event_time - ticks_;

            if (cnextevt < cexecuted + dc) {
                dc = static_cast<int>(cnextevt - cexecuted);

                if (dc < 0) {
                    dc = 0;
                }
            }
        }

        idle_ticks_ += dc;
        downcount_ -= dc;
    }

    void ntimer::add_ticks(uint32_t ticks) {
        downcount_ -= ticks;
    }

    void ntimer::advance() {
        move_events();

        auto org_slice = slice_len_;
        auto org_timer = ticks_;

        const std::int64_t cycles_executed = slice_len_ - downcount_;
        ticks_ += cycles_executed;
        slice_len_ = INITIAL_SLICE_LENGTH;

        timer_sane_ = true;

        while (!events_.empty() && events_.back().event_time <= ticks_) {
            event evt = std::move(events_.back());
            events_.pop_back();

            std::stable_sort(events_.begin(), events_.end(), [](const event &lhs, const event &rhs) {
                return lhs.event_time > rhs.event_time;
            });

            {
                const std::lock_guard<std::mutex> lock(timing_->mut_);
                timing_->event_types_[evt.event_type]
                    .callback(evt.event_user_data, static_cast<int>(ticks_ - evt.event_time));
            }
        }

        timer_sane_ = false;

        if (!events_.empty()) {
            slice_len_ = common::min<std::int64_t>(static_cast<std::int64_t>(events_.back().event_time - ticks_),
                static_cast<std::int64_t>(MAX_SLICE_LENGTH));
        }

        downcount_ = slice_len_;
    }

    void ntimer::schedule_event(int64_t cycles_into_future, int event_type, std::uint64_t userdata,
        const bool thr_safe) {
        event evt;

        evt.event_time = ticks_ + cycles_into_future;
        evt.event_type = event_type;
        evt.event_user_data = userdata;

        if (thr_safe) {
            ts_events_.push(evt);
        } else {
            events_.push_back(evt);

            std::stable_sort(events_.begin(), events_.end(), [](const event &lhs, const event &rhs) {
                return lhs.event_time > rhs.event_time;
            });
        }
    }

    bool ntimer::unschedule_event(int event_type, uint64_t userdata) {
        auto res = std::find_if(events_.begin(), events_.end(),
            [&](auto evt) { return (evt.event_type == event_type) && (evt.event_user_data == userdata); });

        if (res != events_.end()) {
            events_.erase(res);

            std::stable_sort(events_.begin(), events_.end(), [](const event &lhs, const event &rhs) {
                return lhs.event_time > rhs.event_time;
            });

            return true;
        } else {
            const std::lock_guard guard(ts_events_.lock);
            
            for (decltype(ts_events_)::iterator ite = ts_events_.begin(); ite != ts_events_.end(); ite++) {
                if (ite->event_user_data == userdata) {
                    ts_events_.erase(ite);
                    return true;
                }
            }
        }

        return false;
    }

    void timing_system::fire_mhz_changes() {
        for (auto &mhz_change : internal_mhzcs_) {
            mhz_change();
        }
    }

    ntimer *timing_system::current_timer() {
        return timers_[current_core_].get();
    }

    void timing_system::set_clock_frequency_mhz(int cpu_mhz) {
        CPU_HZ = cpu_mhz;
        fire_mhz_changes();
    }

    std::uint32_t timing_system::get_clock_frequency_mhz() {
        return static_cast<std::uint32_t>(CPU_HZ / 1000000);
    }

    const std::int64_t timing_system::downcount() {
        return current_timer()->downcount();
    }
    
    const std::uint64_t timing_system::ticks() {
        return current_timer()->ticks();
    }

    const std::uint64_t timing_system::idle_ticks() {
        return current_timer()->idle_ticks();
    }

    std::uint64_t timing_system::get_global_time_us() {
        return cycles_to_us(ticks());
    }

    int timing_system::register_event(const std::string &name, timed_callback callback) {
        const std::lock_guard<std::mutex> guard(mut_);
        event_type evtype;

        evtype.name = name;
        evtype.callback = callback;

        event_types_.push_back(evtype);

        return static_cast<int>(event_types_.size() - 1);
    }

    int timing_system::get_register_event(const std::string &name) {
        const std::lock_guard<std::mutex> guard(mut_);

        for (uint32_t i = 0; i < event_types_.size(); i++) {
            if (event_types_[i].name == name) {
                return i;
            }
        }

        return -1;
    }

    void timing_system::init(const int total_core) {
        // TODO(pent0): Multicore
        timers_.resize(1);

        for (int i = 0; i < total_core; i++) {
            timers_[i] = std::make_unique<ntimer>(this, i);
        }

        current_core_ = 0;
        CPU_HZ = 484000000;
    }

    void timing_system::shutdown() {

    }

    void timing_system::restore_register_event(int evt_type, const std::string &name, timed_callback callback) {
        event_type evtype;

        evtype.callback = callback;
        evtype.name = name;

        event_types_[evt_type] = evtype;
    }

    void timing_system::unregister_all_events() {
        event_types_.clear();
    }

    void timing_system::add_ticks(uint32_t ticks) {
        current_timer()->add_ticks(ticks);
    }
    
    void timing_system::idle(int max_idle) {
        current_timer()->idle(max_idle);
    }

    void timing_system::advance() {
        current_timer()->advance();
    }

    void timing_system::schedule_event(int64_t cycles_into_future, int event_type, uint64_t userdata,
        const bool thr_safe) {
        current_timer()->schedule_event(cycles_into_future, event_type, userdata, thr_safe);
    }

    bool timing_system::unschedule_event(int event_type, uint64_t usrdata) {
        return current_timer()->unschedule_event(event_type, usrdata);
    }

    void timing_system::register_mhz_change_callback(mhz_change_callback change_callback) {
        std::lock_guard<std::mutex> guard(mut_);
        internal_mhzcs_.push_back(change_callback);
    }
}
