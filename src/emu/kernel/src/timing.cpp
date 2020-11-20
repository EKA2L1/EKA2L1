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
#include <common/thread.h>
#include <common/platform.h>

#include <kernel/timing.h>

#include <algorithm>
#include <mutex>
#include <thread>
#include <vector>

namespace eka2l1 {
    ntimer::ntimer(const std::uint32_t cpu_hz) {
        CPU_HZ_ = cpu_hz;
        should_stop_ = false;
        should_paused_ = false;
        acc_level_ = realtime_level_low;

        teletimer_ = common::make_teletimer(cpu_hz);

        timer_thread_ = std::make_unique<std::thread>([this]() {
            loop();
        });

        set_realtime_level(realtime_level_mid);
        teletimer_->start();
    }

    ntimer::~ntimer() {
        should_stop_ = true;
        should_paused_ = false;

        new_event_evt_.set();
        pause_evt_.set();

        timer_thread_->join();
    }

    void ntimer::set_realtime_level(const realtime_level lvl) {
        std::unique_lock<std::mutex> unq(lock_);
        
        if (acc_level_ == lvl) {
            return;
        }

        if (acc_level_ == realtime_level_low) {
            res_guard_.toogle();
        }

        switch (lvl) {
        case realtime_level_low:
            res_guard_.toogle();
            break;

        default:
            break;
        }

        acc_level_ = lvl;
    }

    void ntimer::loop() {
        static const char *TIMING_THREAD_NAME = "Timing thread";

        common::set_thread_name(TIMING_THREAD_NAME);
        common::set_thread_priority(common::thread_priority_very_high);

        while (!should_stop_) {
            while (!should_stop_ && !should_paused_) {
                const std::optional<std::uint64_t> next_microseconds = advance();

                if ((next_microseconds.has_value()) && (acc_level_ < realtime_level_high)) {
#if EKA2L1_PLATFORM(ANDROID)
                    // Snail speed, let it sleep more.
                    constexpr std::uint64_t IGNORE_AND_GO_PASS_MICROSECS = 16;
#else
                    constexpr std::uint64_t IGNORE_AND_GO_PASS_MICROSECS = 32;
#endif

                    if (next_microseconds > IGNORE_AND_GO_PASS_MICROSECS) {
                        new_event_evt_.wait_for(next_microseconds.value());
                    }
                } else {
                    new_event_evt_.wait();
                }
            }

            if (should_paused_) {
                pause_evt_.wait();
            }
        }
    }

    const std::uint64_t ntimer::ticks() {
        return teletimer_->ticks();
    }

    const std::uint64_t ntimer::microseconds() {
        return teletimer_->microseconds();
    }

    std::optional<std::uint64_t> ntimer::advance() {
        std::unique_lock<std::mutex> unq(lock_);
        std::uint64_t global_timer = teletimer_->microseconds();

        while (!events_.empty() && events_.back().event_time <= global_timer) {
            event evt = std::move(events_.back());
            events_.pop_back();

            std::stable_sort(events_.begin(), events_.end(), [](const event &lhs, const event &rhs) {
                return lhs.event_time > rhs.event_time;
            });

            unq.unlock();

            if (event_types_[evt.event_type].callback) {
                event_types_[evt.event_type]
                    .callback(evt.event_user_data, static_cast<int>(global_timer - evt.event_time));
            }

            unq.lock();
        }

        if (!events_.empty()) {
            return static_cast<std::uint64_t>(events_.back().event_time - global_timer);
        }

        return std::nullopt;
    }

    void ntimer::schedule_event(int64_t us_into_future, int event_type, std::uint64_t userdata) {
        const std::lock_guard<std::mutex> guard(lock_);

        event evt;

        evt.event_time = teletimer_->microseconds() + us_into_future;
        evt.event_type = event_type;
        evt.event_user_data = userdata;

        bool should_nof = (events_.empty()) ||  (events_.back().event_time > evt.event_time);
        events_.push_back(evt);

        std::stable_sort(events_.begin(), events_.end(), [](const event &lhs, const event &rhs) {
            return lhs.event_time > rhs.event_time;
        });

        if (should_nof) {
            new_event_evt_.set();
        }
    }

    bool ntimer::unschedule_event(int event_type, uint64_t userdata) {
        const std::lock_guard<std::mutex> guard(lock_);

        auto res = std::find_if(events_.begin(), events_.end(),
            [&](auto evt) { return (evt.event_type == event_type) && (evt.event_user_data == userdata); });

        if (res != events_.end()) {
            events_.erase(res);

            std::stable_sort(events_.begin(), events_.end(), [](const event &lhs, const event &rhs) {
                return lhs.event_time > rhs.event_time;
            });

            return true;
        }

        return false;
    }

    bool ntimer::set_clock_frequency_mhz(const std::uint32_t cpu_mhz) {
        if (teletimer_->set_target_frequency(cpu_mhz * 10000000)) {
            CPU_HZ_ = cpu_mhz * 10000000;
            return true;
        }

        return false;
    }

    std::uint32_t ntimer::get_clock_frequency_mhz() {
        return static_cast<std::uint32_t>(CPU_HZ_ / 1000000);
    }

    int ntimer::register_event(const std::string &name, timed_callback callback) {
        const std::lock_guard<std::mutex> guard(lock_);

        event_type evtype;

        evtype.name = name;
        evtype.callback = callback;

        for (std::size_t i = 0; i < event_types_.size(); i++) {
            if (event_types_[i].callback == nullptr) {
                event_types_[i] = evtype;
                return static_cast<int>(i);
            }
        }

        event_types_.push_back(evtype);
        return static_cast<int>(event_types_.size() - 1);
    }

    void ntimer::remove_event(int event_type) {
        const std::lock_guard<std::mutex> guard(lock_);
        if (event_types_.size() <= event_type) {
            return;
        }

        event_types_[event_type].callback = nullptr;
    }

    int ntimer::get_register_event(const std::string &name) {
        const std::lock_guard<std::mutex> guard(lock_);

        for (uint32_t i = 0; i < event_types_.size(); i++) {
            if (event_types_[i].name == name) {
                return i;
            }
        }

        return -1;
    }

    void ntimer::unregister_all_events() {
        event_types_.clear();
    }

    bool ntimer::is_paused() const {
        return should_paused_.load();
    }

    void ntimer::set_paused(const bool should_pause) {
        bool last_state = should_paused_;
        should_paused_ = should_pause;

        if (last_state != should_pause) {
            if (should_pause == false) {
                pause_evt_.set();
            }
        }
    }
    
    realtime_level get_realtime_level_from_string(const char *c) {
        const std::string str = common::lowercase_string(std::string(c));

        if (str == "low") {
            return realtime_level_low;
        }

        if (str == "mid") {
            return realtime_level_mid;
        }

        if (str == "high") {
            return realtime_level_high;
        }
        
        return realtime_level_mid;
    }
    
    const char *get_string_of_realtime_level(const realtime_level lvl) {
        switch (lvl) {
        case realtime_level_low:
            return "low";

        case realtime_level_high:
            return "high";

        case realtime_level_mid:
            return "mid";

        default:
            break;
        }

        return nullptr;
    }
}
