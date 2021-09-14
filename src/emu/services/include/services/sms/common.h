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

#include <services/msv/common.h>

#include <utils/des.h>
#include <cstdint>
#include <memory>

namespace eka2l1::epoc::sms {
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

    enum mobile_store_status {
        mobile_store_status_unknown = 0,
        mobile_store_status_unread = 1,
        mobile_store_status_read = 2,
        mobile_store_status_unsent = 3,
        mobile_store_status_sent = 4,
        mobile_store_status_delivered = 5
    };

    enum sms_pdu_type : std::uint8_t {
        sms_pdu_type_deliver = 0,
        sms_pdu_type_deliver_report = 1,
        sms_pdu_type_submit = 2,
        sms_pdu_type_submit_report = 3,
        sms_pdu_type_status_report = 4,
        sms_pdu_type_command = 5
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

    // Referencing smutset.h
#pragma pack(push, 1)
    struct progress {
        enum progress_type {
            progress_type_default = 0,
            progress_type_read_sim_params = 1,
            progress_type_write_sim_params = 2,
            progress_type_sending = 4,
            progress_type_scheduling = 5
        };

        std::int32_t error_;
        progress_type type_;
        std::int32_t state_;
        std::int32_t rcp_done_;
        std::int32_t rcp_count_;
        std::int32_t msg_done_;
        std::int32_t msg_count_;
        epoc::buf_static<char16_t, 14> sc_address_;
        std::uint32_t enum_folder_;
    };
#pragma pack(pop)

    struct sms_octet {
        std::uint8_t value_;

        virtual void absorb(common::chunkyseri &seri);
    };

    struct sms_type_of_address: public sms_octet {
    };

    struct sms_first_octet : public sms_octet {
    };

    struct sms_protocol_identifier: public sms_octet {
    };

    struct sms_data_encoding_scheme : public sms_octet {
    };

    struct sms_validity_period {
        sms_first_octet first_octet_;
        std::uint32_t interval_minutes_;

        void absorb(common::chunkyseri &seri);
    };

    struct sms_address {
        sms_type_of_address addr_type_;
        std::u16string buffer_;

        void absorb(common::chunkyseri &seri);
    };

    struct sms_pdu {
        sms_pdu_type pdu_type_;
        sms_address service_center_address_;

        virtual void absorb(common::chunkyseri &seri) = 0;
    };

    struct sms_submit : public sms_pdu {
        sms_first_octet first_octet_;
        sms_octet msg_reference_;
        sms_address dest_addr_;
        sms_protocol_identifier protocol_identifier_;
        sms_data_encoding_scheme encoding_scheme_;
        sms_validity_period validity_period_;

        // The rest don't care
        void absorb(common::chunkyseri &seri) override;
    };

    struct sms_message {
        std::uint32_t flags_;
        mobile_store_status store_status_;
        std::int32_t log_id_;
        std::uint64_t time_;

        std::unique_ptr<sms_pdu> pdu_;
        std::vector<std::uint8_t> dontcare_;

        bool absorb(common::chunkyseri &seri);
    };

    struct sms_header {
        std::vector<sms_number> recipients_;
        std::uint32_t flags_;
        msv::bio_msg_type bio_type_;

        sms_message message_;

        bool absorb(common::chunkyseri &seri);
    };

    static constexpr std::uint32_t MSV_SMS_HEADER_STREAM_UID = 0x10001834;
}