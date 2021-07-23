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

#pragma once

#include <services/ui/view/common.h>
#include <utils/reqsts.h>

#include <mutex>
#include <queue>
#include <vector>

namespace eka2l1::ui::view {
    using custom_message = std::vector<std::uint8_t>;

    struct view_event_and_data {
        view_event evt_;
        custom_message custom_;
    };

    class event_queue {
        std::queue<view_event_and_data> events_;

        epoc::notify_info nof_info_;
        std::uint8_t *buffer_;

        std::mutex lock_;

        custom_message current_custom_;

    public:
        explicit event_queue();

        void queue_event(const view_event &evt, const custom_message &msg = {});
        bool hear(epoc::notify_info info, std::uint8_t *complete_buffer);

        custom_message current_custom_message() {
            return current_custom_;
        }
    };
}