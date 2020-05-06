/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <cstdint>

namespace eka2l1::epoc::msv {
    enum change_notification_type {
        change_notification_type_entries_no = 0,
        change_notification_type_entries_created = 1,
        change_notification_type_entries_changed = 2,
        change_notification_type_entries_deleted = 3,
        change_notification_type_entries_moved = 4,
        
        change_notification_type_mtm_group_installed = 5,
        change_notification_type_mtm_group_deinstalled = 6,

        change_notification_type_store_deleted = 7,
        change_notification_type_close_session = 8,

        change_notification_type_index_loaded = 9,
        change_notification_type_index_load_failed = 10,
        
        change_notification_type_index_media_changed = 12,
        change_notification_type_index_media_unavailable = 13,
        change_notification_type_index_media_available = 14,
        change_notification_type_index_media_incorrect = 15,

        change_notification_type_msg_store_not_supported = 16,
        change_notification_type_msg_store_corrupt = 17,
        change_notification_type_refresh_msg_view = 18,
        change_notification_type_disk_unavail = 19,
        change_notification_type_unable_to_process_disk_nof = 20
    };
    
    static constexpr std::uint32_t MTM_DEFAULT_SPECIFIC_UID = 0x10202D51;
    static constexpr std::uint32_t MTM_SERVICE_UID_ROOT = 0x10000F67;
}