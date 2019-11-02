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

#include <epoc/services/ui/view/common.h>
#include <epoc/utils/reqsts.h>

#include <queue>
#include <mutex>

namespace eka2l1::ui::view {
    class event_queue {
        std::queue<view_event> events_;
        epoc::notify_info nof_info_;
        std::uint8_t *buffer_;

        std::mutex lock_;

    public:
        explicit event_queue();
        
        void queue_event(const view_event &evt);
        bool hear(epoc::notify_info info, std::uint8_t *complete_buffer);
    };
}