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
#include <epoc/utils/reqsts.h>

#include <array>
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace eka2l1::epoc {
    template <typename T, unsigned int MAX_ELEM = 32>
    class base_fifo {
    public:
        enum {
            maximum_element = MAX_ELEM
        };

        enum class queue_priority {
            high,       ///< When the queue is max, all events that can be purged will be purge
            low         ///< When the queue is max, the event will not be schedule, until some elements are freed
        };

        struct fifo_element {
            std::uint32_t id;
            std::uint16_t pri;
            T   evt;

            fifo_element(const std::uint32_t id, const T &evt) 
                : id(id), evt(std::move(evt)) {
            }
        };

    protected:
        std::vector<fifo_element>           q_;
        std::mutex                         lock_;

        epoc::notify_info nof;

        /*! \brief Queue an event. This doesn't care about whenther the queue has reached maximum size
         *         yet
         * 
         * This method is unsafe
        */
        std::uint32_t queue_event_dont_care(const T &evt) {
            fifo_element element(static_cast<std::uint32_t>(q_.size()) + 1, evt);
            q_.push_back(std::move(element));

            // An event is pushed, we should finish notification
            nof.complete(0);

            return static_cast<std::uint32_t>(q_.size());
        }

    public:
        base_fifo() {}

        /*! \brief Set a listener to all events that are going to be queued.
         *
         * If an event is pending, this will mostly returns immidiately with the request finished.
         * 
         * \params nof_info The notification. If the queue is not empty, this will be finished with KErrNone.
         *                  Otherwise, it will be stored until another event is queued.
        */
        void set_listener(epoc::notify_info nof_info) {
            const std::lock_guard<std::mutex> guard(lock_);

            if (q_.size() > 0) {
                // Complete with KErrNone
                nof_info.complete(0);
                return;
            }

            nof = nof_info;
        }

        /*! \brief Cancel an already-queued event.
         *
         * \params id The id of the event.
        */
        void cancel_event_queue(std::uint32_t id) {
            const std::lock_guard<std::mutex> guard(lock_);

            auto elem_ite = std::find_if(q_.begin(), q_.end(), 
                [=](const fifo_element &elem) { return elem.id == id; });

            if (elem_ite != q_.end()) {
                q_.erase(elem_ite);
            }
        }

        /*! \brief Get an event on the queue.
         *
         * \returns nullopt if nothing is on the queue.
        */
        std::optional<T> get_evt_opt() {
            const std::lock_guard<std::mutex> guard(lock_);
            if (q_.size() == 0) {
                return std::nullopt;
            }

            fifo_element elem = std::move(q_.front());
            
            // Keep the order. Not too efficient though
            q_.erase(q_.begin());
            return elem.evt;
        }
    };

    class event_fifo: public base_fifo<event, 32> {
    protected:
        /*! \brief Check if the event is high-priority
         *
         * High priority events are password, switch on and off event.
        */
        bool is_my_priority_really_high(epoc::event_code evt);

        /*! \brief Purge some events on the queue.
         *
         * When the queue reached the maximum declared, it will purge some
         * events (if the condition is met) to free space for other events to be
         * queued.
         * 
         * On hardware, this is to prevent memory being wasted due to no users
         * requests a notification for a long time. We should do this too, there 
         * is no reason not to do it.
        */
        void do_purge();

    public:
        event_fifo() : base_fifo<event>() {}

        /*! \brief Get an on-queue event
         *
         * If there is nothing on the queue, a null event is returned.
        */
        event get_event();

        std::uint32_t queue_event(const event &evt);
    };

    class redraw_fifo: public base_fifo<redraw_event, 32> {
    public:
        redraw_fifo() : base_fifo<redraw_event>() {}
        std::uint32_t queue_event(const redraw_event &evt, const std::uint16_t pri);
    };
}