/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <services/ui/view/queue.h>

namespace eka2l1::ui::view {
    event_queue::event_queue()
        : buffer_(nullptr) {
    }

    static void complete_write_and_notify_event(epoc::notify_info &info, std::uint8_t *dest_buffer,
        const view_event &evt) {
        // Just notify please
        info.complete(0);

        if (dest_buffer) {
            // Write the buffer. A step should have been done to verify buffer is sufficient
            *reinterpret_cast<view_event *>(dest_buffer) = evt;
        }
    }

    void event_queue::queue_event(const view_event &evt) {
        const std::lock_guard<std::mutex> guard(lock_);

        if (!nof_info_.empty()) {
            complete_write_and_notify_event(nof_info_, buffer_, evt);
            buffer_ = nullptr;

            return;
        }

        // Queue the event
        events_.push(evt);
    }

    bool event_queue::hear(epoc::notify_info info, std::uint8_t *buffer) {
        const std::lock_guard<std::mutex> guard(lock_);

        if (!nof_info_.empty() || buffer_) {
            // Previous not finish, not allowed
            return false;
        }

        if (!events_.empty()) {
            auto evt = std::move(events_.front());
            events_.pop();

            complete_write_and_notify_event(info, buffer, evt);
            return true;
        }

        // Queue this nof
        nof_info_ = info;
        buffer_ = buffer;

        return true;
    }
}