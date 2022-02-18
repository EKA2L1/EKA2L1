/*
 * Copyright (c) 2021 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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

#include <array>
#include <cstdint>
#include <vector>
#include <string>

namespace eka2l1::common {
    class chunkyseri;
}

namespace eka2l1::epoc::drm {
    enum rights_type {
        rights_type_none = 0,
        rights_type_play = 1 << 0,
        rights_type_display = 1 << 1,
        rights_type_execute = 1 << 2,
        rights_type_print = 1 << 3,
        rights_type_export = 1 << 4,
        rights_type_top_level = 1 << 5
    };

    enum rights_intent {
        rights_intent_peek = 0,
        rights_intent_play = 1,
        rights_intent_view = 2,
        rights_intent_execute = 3,
        rights_intent_print = 4,
        rights_intent_pause = 5,
        rights_intent_continue = 6,
        rights_intent_stop = 7,
        rights_intent_unknown = 8,
        rights_intent_install = 9
    };

    enum rights_constraint_type {
        rights_constraint_none = 0,
        rights_constraint_start_time = 1 << 0,
        rights_constraint_end_time = 1 << 1,
        rights_constraint_interval = 1 << 2,
        rights_constraint_counter = 1 << 3,
        rights_constraint_top_level = 1 << 4,
        rights_constraint_timed_counter = 1 << 5,
        rights_constraint_accumulated = 1 << 6,
        rights_constraint_individual = 1 << 7,
        rights_constraint_system = 1 << 8,
        rights_constraint_vendor = 1 << 9,
        rights_constraint_software = 1 << 10,
        rights_constraint_metering = 1 << 11
    };

    enum rights_export_mode : std::uint8_t {
        rights_export_mode_none,
        rights_export_mode_copy,
        rights_export_mode_move
    };

    struct rights_constraint {
        std::uint64_t start_time_ = 0;
        std::uint64_t end_time_ = 0;
        std::uint64_t interval_start_ = 0;
        std::uint32_t interval_ = 0;
        std::uint32_t counter_ = 0;
        std::uint32_t original_counter_ = 0;
        std::uint32_t timed_counter_ = 0;
        std::uint32_t timed_interval_ = 0;
        std::uint32_t accumulated_time_ = 0;
        std::string individual_;
        std::uint32_t vendor_id_ = 0;
        std::uint32_t secure_id_ = 0;
        std::uint32_t active_constraints_ = 0;
        std::string system_;
        std::uint32_t metering_grace_time_ = 0;
        std::uint8_t allow_use_without_metering_ = 0;
        std::uint32_t original_timed_counter_ = 0;

        void absorb(common::chunkyseri &seri, std::uint32_t version);
    };

    struct rights_permission {
        std::uint32_t unique_id_ = 0;
        std::uint64_t insert_time_ = 0;
        
        rights_constraint top_level_constraint_;
        rights_constraint play_constraint_;
        rights_constraint display_constraint_;
        rights_constraint execute_constraint_;
        rights_constraint print_constraint_;
        rights_constraint export_contraint_;

        rights_export_mode export_mode_ = rights_export_mode_none;
        std::string parent_rights_id_;
        std::string rights_obj_id;
        std::string domain_id_;

        std::uint16_t available_rights_ = 0;
        std::uint16_t version_rights_main_ = 1;
        std::uint16_t version_rights_sub_ = 0;

        std::uint32_t info_bits_ = 0;
        std::array<std::uint8_t, 20> right_issuer_identifier_;       // 20 in length
        std::string on_expired_url_;

        void absorb(common::chunkyseri &seri, std::uint32_t version);
        rights_constraint *get_constraint_from_intent(const rights_intent intent);
        const bool software_constrained() const;
    };

    struct rights_common_data {
        std::string content_id_;
        std::string content_hash_;
        std::string issuer_;
        std::string content_name_;
        std::string auth_seed_;
    };

    struct rights_object {
        rights_common_data common_data_;
        std::string encrypt_key_;

        std::vector<rights_permission> permissions_;
    };
}