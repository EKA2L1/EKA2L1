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

#include <common/uid.h>

namespace eka2l1::ui::view {
    struct view_id {
        epoc::uid app_uid; ///< The UID of the app owning this view.
        epoc::uid view_uid; ///< The view UID.

        inline bool operator == (const view_id &rhs) {
            return (app_uid == rhs.app_uid) && (view_uid == rhs.view_uid);
        }
    };

    struct view_event {
        enum event_type {
            event_active_view = 0,
            event_deactive_view = 1,
            event_screen_device_change = 2,
            event_deactive_notification = 3,
            event_active_notification = 4,
            event_deactive_view_different_instance = 5
        };

        event_type view_event_type;
        view_id view_one_id;
        view_id view_two_id;
        epoc::uid custom_message_id;
        std::int32_t custom_message_length;
    };
}