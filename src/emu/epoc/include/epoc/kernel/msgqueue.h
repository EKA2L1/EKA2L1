/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#pragma once

#include <epoc/kernel/kernel_obj.h>
#include <epoc/utils/reqsts.h>

#include <functional>
#include <queue>
#include <vector>

namespace eka2l1::kernel {
    using msg_queue_callback = std::function<void(void *)>;
    using msg_queue_callback_entry = std::pair<msg_queue_callback, void *>;

    class thread;

    class msg_queue : public kernel_obj {
        std::uint32_t max_msg_length_;
        std::uint32_t max_length_;

        using msg_data = std::vector<std::uint8_t>;

        std::queue<msg_data> msgs_;

        std::vector<epoc::notify_info> avail_notifies_;
        std::vector<epoc::notify_info> full_notifies_;

        msg_queue_callback_entry avail_callback_;

    public:
        explicit msg_queue(eka2l1::kernel_system *kern, const std::string &name,
            const std::uint32_t max_message_size, const std::uint32_t max_length);

        const std::uint32_t max_message_length() const {
            return max_msg_length_;
        }

        bool notify_available(epoc::notify_info &info);
        bool notify_full(epoc::notify_info &info);

        void cancel_data_available(kernel::thread *requester);

        void clear_available_callback();
        void set_available_callback(msg_queue_callback callback, void *userdata);

        bool receive(void *target_buffer, const std::uint32_t buffer_size);
        bool send(const void *target_data, const std::uint32_t buffer_size);
    };
}