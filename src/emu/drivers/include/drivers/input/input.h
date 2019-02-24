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

#pragma once

#include <common/vecx.h>
#include <drivers/driver.h>
#include <drivers/input/common.h>

#include <cstdint>
#include <mutex>
#include <queue>

namespace eka2l1::drivers {
    enum input_driver_opcode {
        input_driver_get_events,
        input_driver_lock, ///< Lock access to the queue, prevent from pushing event until release
        input_driver_release, ///< Release lock from the queue
        input_driver_get_total_events
    };

    class input_driver : public driver {
        std::uint32_t flags_;

        enum {
            input_active = 0x1000,
            locked = 0x2000
        };

        std::mutex lock_;

        std::queue<input_event> events_;
        std::queue<input_event> pending_locked_events_;

    protected:
        void move_events();

    public:
        /**
         * \brief Set the driver to be active or inactive.
         * 
         * When being inactive, the driver will not receive and queue any input
         * events.
         */
        void set_active(const bool active) {
            const std::lock_guard<std::mutex> guard_(lock_);

            flags_ &= ~input_active;
            if (active) {
                flags_ |= input_active;
            }
        }

        bool is_active() const {
            return flags_ & input_active;
        }

        void queue_key_event(const int raw_mapped_keycode, const key_state action);

        /**
         * Get an queued key event.
         */
        bool get_event(input_event *evt);

        void process_requests() override;
    };
}