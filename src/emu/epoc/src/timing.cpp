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
    ntimer::ntimer(const std::uint32_t cpu_hz) {
        CPU_HZ_ = cpu_hz;
        should_stop_ = false;
        should_paused_ = false;
        teletimer_ = common::make_teletimer(cpu_hz);

        timer_thread_ = std::make_unique<std::thread>([this]() {
            loop();
        });

        teletimer_->start();
    }

    ntimer::~ntimer() {
        should_stop_ = true;
        should_paused_ = true;
        new_event_avail_var_.notify_one();

        timer_thread_->join();
    }

    void ntimer::loop() {
        while (!should_stop_) {
            while (!should_paused_) {
                const std::optional<std::uint64_t> next_microseconds = advance();

                if (next_microseconds) {
                    std::this_thread::sleep_for(std::chrono::microseconds(next_microseconds.value()));
                } else {
                    std::unique_lock<std::mutex> unqlock(new_event_avail_lock_);
                    new_event_avail_var_.wait(unqlock);
                }
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
            event_types_[evt.event_type]
                .callback(evt.event_user_data, static_cast<int>(global_timer - evt.event_time));
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

        bool was_empty = (events_.empty());
        events_.push_back(evt);

        std::stable_sort(events_.begin(), events_.end(), [](const event &lhs, const event &rhs) {
            return lhs.event_time > rhs.event_time;
        });

        if (was_empty) {
            new_event_avail_var_.notify_one();
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
        should_paused_ = should_pause;
    }
}
