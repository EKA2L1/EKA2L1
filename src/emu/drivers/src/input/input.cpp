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
    void input_driver::queue_key_event(const int raw_mapped_keycode, const key_state action) {
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

        if (flags_ & locked) {
            pending_locked_events_.emplace(std::move(evt));
        } else {    
            events_.emplace(std::move(evt));
        }
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

    void input_driver::move_events() {
        while (!pending_locked_events_.empty()) {
            auto pending_event = std::move(pending_locked_events_.front());
            pending_locked_events_.pop();

            events_.push(std::move(pending_event));
        }
    }
    
    void input_driver::process_requests() {
        for (;;) {
            auto request = request_queue.pop();

            if (!request) {
                break;
            }

            switch (request->opcode) {
            case input_driver_lock: {
                flags_ |= locked;
                break;
            }

            case input_driver_release: {
                flags_ &= ~locked;
                move_events();
                break;
            }

            case input_driver_get_events: {
                input_event *evt = *request->context.pop<input_event*>();
                std::uint32_t num = *request->context.pop<std::uint32_t>();

                std::uint32_t i = 0;

                while ((i < num) && get_event(evt + i++));         
                break;
            }

            case input_driver_get_total_events: {
                std::uint32_t *num = *request->context.pop<std::uint32_t*>();
                *num = static_cast<std::uint32_t>(events_.size());

                break;
            }

            default: {
                LOG_ERROR("Unsupported input driver opcode {}", request->opcode);
                break;
            }
            }
        }

        cond.notify_all();
    }
}
