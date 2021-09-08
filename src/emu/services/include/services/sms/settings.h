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

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1 {
    namespace common {
        class chunkyseri;
    }

    class system;
}

namespace eka2l1::epoc {
    enum sms_pid_conversion : std::uint16_t {
        sms_pid_conversion_none = 0,
        sms_pid_conversion_fax = 2,
        sms_pid_conversion_x400 = 0x11,
        sms_pid_conversion_paging = 0x06,
        sms_pid_conversion_mail = 0x12,
        sms_pid_conversion_ermes = 0x05,
        sms_pid_conversion_speech = 0x04
    };

    enum sms_encoding_alphabet : std::uint16_t {
        sms_encoding_alphabet_7bit = 0,
        sms_encoding_alphabet_8bit = 4,
        sms_encoding_alphabet_ucs2 = 8,
        sms_encoding_alphabet_reserved = 0xC
    };

    enum sms_time_validity_period_format : std::uint8_t {
        sms_vpf_none = 0x0,
        sms_vpf_enhanced = 8,           // 7 octets
        sms_vpf_integer = 0x10,         // Relative
        sms_vpf_semi_octet = 0x18       // Absolute
    };

    enum sms_delivery : std::uint8_t {
        sms_delivery_immediately = 0,
        sms_delivery_upon_request = 1,
        sms_delivery_scheduled = 2
    };

    enum sms_report_handling : std::uint8_t {
        sms_report_to_inbox_invisible = 0,
        sms_report_to_inbox_visible = 1,
        sms_report_discard = 2,
        sms_report_do_not_watch = 3,
        sms_report_to_inbox_invisible_and_match = 4,
        sms_report_to_inbox_visible_and_match = 5,
        sms_report_discard_and_match = 6
    };

    enum sms_commdb_action : std::uint8_t {
        sms_commdb_no_action = 0,
        sms_commdb_store = 1
    };

    enum mobile_sms_bearer : std::uint8_t {
        mobile_sms_bearer_packet_only,
        mobile_sms_bearer_circuit_only,
        mobile_sms_bearer_packet_preferred,
        mobile_sms_bearer_circuit_perferred
    };

    enum sms_acknowledge_status {
        sms_no_acknowledge = 0,
        sms_pending_acknowledge = 1,
        sms_successful_acknowledge = 2,
        sms_error_acknowledge = 3
    };

    enum recipient_status {
        recipient_status_not_yet_sent = 0,
        recipient_status_sent_successfully = 1,
        recipient_status_failed_to_send = 2
    };

    struct sms_number {
        recipient_status recp_status_;
        std::int32_t error_;
        std::int32_t retries_;

        std::uint64_t time_;

        std::u16string number_;
        std::u16string name_;

        std::uint32_t log_id_;
        sms_acknowledge_status delivery_status_;

        explicit sms_number();
        void absorb(common::chunkyseri &seri);
    };

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

    void supply_sim_settings(eka2l1::system *sys);
}