/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <epoc/services/window/common.h>

#include <cstdint>
#include <algorithm>
#include <mutex>
#include <optional>
#include <vector>

namespace eka2l1::epoc {
    template <typename T>
    class base_fifo {
    protected:
        std::vector<T> q_;
        std::mutex     lock_;

    public:
        base_fifo() {}

        std::uint32_t queue_event(const T &evt) {
            const std::lock_guard<std::mutex> guard(lock_);
            const auto elem_ite = std::find_if(q_.begin(), q_.end(),
                [&](const T &e) { return e.handle == evt.handle; });
            
            if (elem_ite != q_.end()) {
                return 0; 
            }

            q_.push_back(evt);
            q_.back().handle = static_cast<std::uint32_t>(q_.size());

            return static_cast<std::uint32_t>(q_.size());
        }

        void cancel_event_queue(std::uint32_t id) {
            const std::lock_guard<std::mutex> guard(lock_);
            const auto elem_ite = std::find_if(q_.begin(), q_.end(),
                [&](const T &e) { return e.handle == id; });
            
            if (elem_ite == q_.end()) {
                return; 
            }

            q_.erase(elem_ite);
        }

        std::optional<T> get_evt_opt() {
            const std::lock_guard<std::mutex> guard(lock_);
            if (q_.size() == 0) {
                return std::nullopt;
            }

            T evt = std::move(q_.front());
            
            // Keep the order. Not too efficient though
            q_.erase(q_.begin());
            return evt;
        }
    };

    class event_fifo: public base_fifo<event> {
    public:
        event_fifo() : base_fifo<event>() {}
        event get_event();
    };

    class redraw_fifo: public base_fifo<redraw_event> {
    public:
        redraw_fifo() : base_fifo<redraw_event>() {}
    };
}