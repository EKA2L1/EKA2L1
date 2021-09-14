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

#include <common/types.h>
#include <common/uid.h>

#include <cstdint>
#include <utils/des.h>

#include <vector>

namespace eka2l1::epoc::msv {
    enum change_notification_type {
        change_notification_type_entries_no = 0,
        change_notification_type_entries_created = 1,
        change_notification_type_entries_changed = 2,
        change_notification_type_entries_deleted = 3,
        change_notification_type_entries_moved = 4,

        change_notification_type_mtm_group_installed = 5,
        change_notification_type_mtm_group_uninstalled = 6,

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

    enum local_operation {
        local_operation_none = 0,
        local_operation_move = 1,
        local_operation_delete = 2,
        local_operation_copy = 3,
        local_operation_new = 4,
        local_operation_changed = 5
    };

    enum send_state {
        send_state_in_preparation = 0,
        send_state_waiting_to_send = 1,
        send_state_connecting = 2,
        send_state_sending = 3,
        send_state_done = 4,
        send_state_failed = 5
    };

    enum bio_msg_type : std::uint8_t {
        bio_msg_id_unknown,
        bio_msg_id_iana,
        bio_msg_id_nbs,
        bio_msg_id_wap,
        bio_msg_id_wap_secure,
        bio_msg_id_wsp,
        bio_msg_id_wsp_secure
    };

#pragma pack(push, 1)
    struct local_operation_progress {
        local_operation operation_ = local_operation_none;
        std::int32_t number_of_entries_ = 0;
        std::int32_t number_completed_ = 0;
        std::int32_t number_failed_ = 0;
        std::int32_t number_remaining_ = 0;
        std::int32_t error_;

        std::uint32_t mid_;
    };
#pragma pack(pop)

#pragma pack(push, 1)
    struct entry_data {
        std::uint32_t id_;
        std::uint32_t parent_id_;
        std::uint32_t data_;
        std::int32_t pc_sync_count_;
        std::int32_t reserved_;

        std::uint32_t service_id_;
        std::uint32_t related_id_;
        std::uint32_t type_uid_;
        std::uint32_t mtm_uid_;
        std::uint64_t date_;

        std::int32_t size_;
        std::int32_t error_;
        std::int32_t bio_type_;

        std::int32_t mtm_datas_[3];

        epoc::ptr_desc16 description_;
        epoc::ptr_desc16 details_;
    };
#pragma pack(pop)

    static_assert(sizeof(entry_data) == 84, "Entry data size is not binary compatible");

    struct entry {
        std::uint32_t id_ = 0;
        std::int32_t parent_id_ = 0;
        std::uint32_t service_id_ = 0;
        std::uint32_t related_id_ = 0;
        std::uint32_t visible_id_ = 0;
        std::int32_t pc_sync_count_ = 0;
        epoc::uid type_uid_ = 0;
        epoc::uid mtm_uid_ = 0;
        std::uint32_t data_ = 0;
        std::u16string description_;
        std::u16string details_;
        std::uint64_t time_ = 0;

        std::int32_t size_ = 0;
        std::int32_t error_ = 0;
        std::int32_t bio_type_ = 0;
        std::int32_t reserved_ = 0;

        std::int32_t mtm_datas_[3];

        std::vector<std::uint32_t> children_ids_;

        std::uint32_t emulator_flags_ = 0;
        std::uint32_t read_count_ = 0;

        enum emulator_flag {
            EMULATOR_FLAG_CHILDREN_LOOKED = 1 << 0,
            EMULATOR_FLAG_STORE_LOCK = 1 << 1,
            EMULATOR_FLAG_ENTRY_LOCK = 1 << 2
        };

        enum data_flag {
            DATA_FLAG_INCOMPLETE = 0x8,
            DATA_FLAG_NEW = 0x10,
            DATA_FLAG_UNREAD = 0x20,
            DATA_FLAG_INVISIBLE = 0x200,
            DATA_FLAG_MULTI_RECIPIENTS = 0x400,
            DATA_FLAG_SCHEDULED = 0x800,
            DATA_FLAG_READ_ONLY = 0x1000,
            DATA_FLAG_DELETED = 0x2000,
            DATA_FLAG_ATTACHMENT = 0x8000,
            DATA_FLAG_VISIBLE_FOLDER_FLAG = 0x04000000
        };

        bool children_looked_up() const {
            return emulator_flags_ & EMULATOR_FLAG_CHILDREN_LOOKED;
        }

        void children_looked_up(bool value) {
            emulator_flags_ &= ~EMULATOR_FLAG_CHILDREN_LOOKED;
            if (value) {
                emulator_flags_ |= EMULATOR_FLAG_CHILDREN_LOOKED;
            }
        }

        const bool store_lock() const {
            return emulator_flags_ & EMULATOR_FLAG_STORE_LOCK;
        }

        void store_lock(const bool value) {
            emulator_flags_ &= ~EMULATOR_FLAG_STORE_LOCK;
            if (value) {
                emulator_flags_ |= EMULATOR_FLAG_STORE_LOCK;
            }
        }

        const bool locked() const {
            return emulator_flags_ & EMULATOR_FLAG_ENTRY_LOCK;
        }

        void lock() {
            emulator_flags_ |= EMULATOR_FLAG_STORE_LOCK;
        }

        void unlock() {
            emulator_flags_ &= ~EMULATOR_FLAG_STORE_LOCK;
        }

        const bool visible() const {
            return !(data_ & DATA_FLAG_INVISIBLE);
        }

        void visible(const bool value) {
            data_ &= ~DATA_FLAG_INVISIBLE;
            if (!value) {
                data_ |= DATA_FLAG_INVISIBLE;
            }
        }

        const bool visible_folder() const {
            return data_ & DATA_FLAG_VISIBLE_FOLDER_FLAG;
        }

        void visible_folder(const bool value) {
            data_ &= ~DATA_FLAG_VISIBLE_FOLDER_FLAG;
            if (value) {
                data_ |= DATA_FLAG_VISIBLE_FOLDER_FLAG;
            }
        }
    };

#pragma pack(push, 1)
    struct system_progress_info {
        std::int32_t version_ = 1;
        std::int32_t err_code_ = 0;
        std::uint32_t id_ = 0;
    };
#pragma pack(pop)

    using msv_id = std::uint32_t;
    using operation_buffer = std::vector<std::uint8_t>;

    static constexpr std::uint32_t MTM_DEFAULT_SPECIFIC_UID = 0x10202D51;
    static constexpr std::uint32_t MTM_SERVICE_UID_ROOT = 0x10000F67;
    static constexpr std::uint32_t MSV_SERVICE_UID = 0x10000F68;
    static constexpr std::uint32_t MSV_FOLDER_UID = 0x10000F69;
    static constexpr std::uint32_t MSV_NORMAL_UID = 0x10000F6A;
    static constexpr std::uint32_t MSV_LOCAL_SERVICE_MTM_UID = 0x10000F71;
    static constexpr std::uint32_t MSV_ROOT_ID_VALUE = 0x1000;
    static constexpr std::uint32_t MSV_LOCAL_SERVICE_ID_VALUE = 0x1001;
    static constexpr std::uint32_t MSV_DRAFT_ENTRY_ID_VALUE = 0x1004;
    static constexpr std::uint32_t MSV_SENT_ENTRY_ID_VALUE = 0x1005;
    static constexpr std::uint32_t MSV_FIRST_FREE_ENTRY_ID = 0x100000;
    static constexpr std::uint32_t MSV_MSG_TYPE_UID = 0x1000102C;
}