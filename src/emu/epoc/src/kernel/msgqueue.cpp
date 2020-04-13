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

#include <epoc/kernel.h>
#include <epoc/kernel/msgqueue.h>

namespace eka2l1::kernel {
    msg_queue::msg_queue(eka2l1::kernel_system *kern, const std::string &name,
        const std::uint32_t max_message_size, const std::uint32_t max_length)
        : kernel_obj(kern, name)
        , max_msg_length_(max_message_size)
        , max_length_(max_length) {
    }

    bool msg_queue::notify_available(epoc::notify_info &info) {
        kern->lock();

        if (!msgs_.empty()) {
            // Complete the request right away
            info.complete(0);
            kern->unlock();

            return true;
        }

        // Find yet if this notification is already registered for the request thread
        auto find_res = std::find_if(avail_notifies_.begin(), avail_notifies_.end(),
            [=](const epoc::notify_info &target_info) {
                return target_info.requester == info.requester;
            });

        if (find_res == avail_notifies_.end()) {
            avail_notifies_.push_back(info);
            kern->unlock();

            return true;
        }

        kern->unlock();
        return false;
    }

    bool msg_queue::notify_full(epoc::notify_info &info) {
        kern->lock();

        if (msgs_.size() == max_length_) {
            // Complete the request right away
            info.complete(0);
            kern->unlock();

            return true;
        }

        // Find yet if this notification is already registered for the request thread
        auto find_res = std::find_if(full_notifies_.begin(), full_notifies_.end(),
            [=](const epoc::notify_info &target_info) {
                return target_info.requester == info.requester;
            });

        if (find_res == full_notifies_.end()) {
            full_notifies_.push_back(info);
            kern->unlock();

            return true;
        }

        kern->unlock();
        return false;
    }

    bool msg_queue::receive(void *target_buffer, const std::uint32_t buffer_size) {
        if (buffer_size != max_msg_length_) {
            return false;
        }

        kern->lock();

        if (msgs_.empty()) {
            kern->unlock();
            return false;
        }

        msg_data &data = msgs_.front();
        std::copy(data.begin(), data.end(), reinterpret_cast<std::uint8_t *>(target_buffer));
        msgs_.pop();

        kern->unlock();

        return true;
    }

    bool msg_queue::send(const void *target_data, const std::uint32_t buffer_size) {
        if (buffer_size > max_msg_length_) {
            return false;
        }

        kern->lock();

        if (msgs_.size() == max_length_) {
            kern->unlock();
            return false;
        }

        if (msgs_.empty()) {
            // Notify all available data
            for (auto &notify : avail_notifies_) {
                notify.complete(0);
            }
        }

        // Push new message
        msg_data new_data;
        new_data.resize(max_msg_length_);

        const std::uint8_t *target_data_to_copy = reinterpret_cast<const std::uint8_t *>(target_data);
        std::copy(target_data_to_copy, target_data_to_copy + buffer_size, &new_data[0]);

        // Push it
        msgs_.push(new_data);

        // Check if full, then notify
        if (msgs_.size() == max_length_) {
            // Notify all available data
            for (auto &notify : full_notifies_) {
                notify.complete(0);
            }
        }

        kern->unlock();

        return true;
    }
}