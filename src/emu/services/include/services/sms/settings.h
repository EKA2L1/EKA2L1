/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <services/sms/common.h>

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1 {
    namespace common {
        class chunkyseri;
    }

    class system;
}

namespace eka2l1::epoc::sms {
    struct sms_message_settings {
        std::uint32_t msg_flags_;
        sms_pid_conversion message_conversion_;
        sms_encoding_alphabet alphabet_;
        std::uint32_t validty_period_minutes_;
        sms_time_validity_period_format validity_period_format_;

        explicit sms_message_settings();
        virtual void absorb(common::chunkyseri &seri);
    };

    struct sms_settings: public sms_message_settings {
        std::uint32_t set_flags_;
        std::int32_t default_sc_index_;
        std::vector<sms_number> sc_numbers_;        // Service center numbers
        sms_delivery delivery_;
        sms_report_handling status_report_handling_;
        sms_report_handling special_message_handling_;
        sms_commdb_action commdb_action_;
        sms_commdb_action bearer_action_;
        mobile_sms_bearer sms_bearer_;

        std::uint32_t external_save_count_;
        std::uint32_t class2_;
        std::uint32_t desc_length_;
        
        explicit sms_settings();
        void absorb(common::chunkyseri &seri) override;
    };

    static const char16_t *SMS_SETTING_PATH = u"C:\\System\\Data\\sms_settings.dat";

    void supply_sim_settings(eka2l1::system *sys, sms_settings *furnished = nullptr);
}