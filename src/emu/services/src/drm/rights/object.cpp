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

#include <services/drm/rights/object.h>
#include <common/chunkyseri.h>

namespace eka2l1::epoc::drm {
    static constexpr std::uint32_t SYNC_MARK = 0xAFCE;

    void rights_constraint::absorb(common::chunkyseri &seri, std::uint32_t version) {
        std::uint32_t sync_mark_temp = SYNC_MARK;
        seri.absorb(sync_mark_temp);

        if (seri.get_seri_mode() == common::SERI_MODE_READ) {
            if (sync_mark_temp == SYNC_MARK) {
                seri.absorb(version);
            } else {
                seri.backwards(sizeof(sync_mark_temp));
            }
        } else {
            if (version == 1) {
                seri.absorb(version);
            }
        }

        seri.absorb(start_time_);
        seri.absorb(end_time_);
        seri.absorb(interval_start_);
        seri.absorb(interval_);
        seri.absorb(counter_);
        seri.absorb(original_counter_);
        seri.absorb(timed_counter_);
        seri.absorb(timed_interval_);
        seri.absorb(accumulated_time_);
        seri.absorb(individual_);   // U32 size

        if (version == 0) {
            seri.absorb(system_);
        }

        seri.absorb(vendor_id_);
        seri.absorb(secure_id_);
        seri.absorb(active_constraints_);

        if (version == 1) {
            std::uint32_t len = 5;
            seri.absorb(len);
            seri.absorb(metering_grace_time_);
            seri.absorb(allow_use_without_metering_);

            seri.absorb(system_);
            seri.absorb(original_timed_counter_);

            std::uint32_t reserved_len = 0;
            seri.absorb(reserved_len);
        }
    }

    void rights_permission::absorb(common::chunkyseri &seri, std::uint32_t version) { 
        std::uint32_t possible_sync_mark = SYNC_MARK;
        seri.absorb(possible_sync_mark);

        if ((seri.get_seri_mode() == common::SERI_MODE_READ) && (possible_sync_mark != SYNC_MARK)) {
            unique_id_ = possible_sync_mark;
            version = 0;
        } else {
            seri.absorb(version);
            seri.absorb(unique_id_);
        }

        seri.absorb(insert_time_);
        top_level_constraint_.absorb(seri, version);
        play_constraint_.absorb(seri, version);
        display_constraint_.absorb(seri, version);
        execute_constraint_.absorb(seri, version);
        print_constraint_.absorb(seri, version);
        export_contraint_.absorb(seri, version);

        seri.absorb(export_mode_);
        seri.absorb(parent_rights_id_);
        seri.absorb(rights_obj_id);
        seri.absorb(domain_id_);
        seri.absorb(available_rights_);
        seri.absorb(version_rights_main_);
        seri.absorb(version_rights_sub_);
        seri.absorb(info_bits_);
        seri.absorb_impl(right_issuer_identifier_.data(), right_issuer_identifier_.size());
        
        if (version == 1) {
            seri.absorb(on_expired_url_);

            std::uint32_t temp_len = 0;
            seri.absorb(temp_len);
        }
    }

    rights_constraint *rights_permission::get_constraint_from_intent(const rights_intent intent) {
        switch (intent) {
        case rights_intent_play:
            if (available_rights_ & rights_type_play)
                return &play_constraint_;

            break;

        case rights_intent_view:
            if (available_rights_ & rights_type_display)
                return &display_constraint_;

            break;
        
        case rights_intent_execute:
            if (available_rights_ & rights_type_execute)
                return &execute_constraint_;

            break;

        case rights_intent_print:
            if (available_rights_ & rights_type_print)
                return &print_constraint_;

            break;

        default:
            break;
        }

        return nullptr;
    }

    const bool rights_permission::software_constrained() const {
        if ((available_rights_ & rights_type_top_level) && (top_level_constraint_.active_constraints_ & (rights_constraint_software | rights_constraint_vendor))) {
            return true;
        }
        
        if ((available_rights_ & rights_type_play) && (play_constraint_.active_constraints_ & (rights_constraint_software | rights_constraint_vendor))) {
            return true;
        }
        
        if ((available_rights_ & rights_type_display) && (display_constraint_.active_constraints_ & (rights_constraint_software | rights_constraint_vendor))) {
            return true;
        }
        
        if ((available_rights_ & rights_type_execute) && (execute_constraint_.active_constraints_ & (rights_constraint_software | rights_constraint_vendor))) {
            return true;
        }
        
        if ((available_rights_ & rights_type_print) && (print_constraint_.active_constraints_ & (rights_constraint_software | rights_constraint_vendor))) {
            return true;
        }

        if ((available_rights_ & rights_type_export) && (export_contraint_.active_constraints_ & (rights_constraint_software | rights_constraint_vendor))) {
            return true; 
        }

        return false;
    }
}