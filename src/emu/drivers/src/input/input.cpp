/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <drivers/input/input.h>
#include <common/log.h>

namespace eka2l1::drivers {
    void input_driver::queue_key_event(const const int raw_mapped_keycode, const key_state action) {
        // The driver will not queue event because it's inactive in receiving inputs
        if (!is_active()) {
            return;
        }

        // Lock and push to the queue
        const std::lock_guard<std::mutex> guard_(lock_);

        input_event evt;
        evt.type_ = input_event_type::key;
        evt.key_.state_ = action;

        // TODO: Really much. We can not just throw a raw keycode. We should also map it, read from a config file or something ?
        evt.key_.code_ = (raw_mapped_keycode == 'A') ? key_mid_button : static_cast<key_scancode>(raw_mapped_keycode);

        events_.emplace(std::move(evt));
    }

    bool input_driver::get_event(input_event *evt) {
        if (!evt) {
            LOG_ERROR("Destination fill event is nullptr!");
            return false;
        }

        // Check if queue is empty. Lock it since we don't want anymore event add in during the time
        // we check for the queue.
        const std::lock_guard<std::mutex> guard_(lock_);

        if (events_.empty()) {
            return false;
        }

        // Get the earliest event. FIFO
        *evt = std::move(events_.front());
        events_.pop();

        return true;
    }
}