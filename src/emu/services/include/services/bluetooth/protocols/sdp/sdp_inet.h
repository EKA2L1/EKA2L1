/*
 * Copyright (c) 2021 EKA2L1 Team
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

#include <services/bluetooth/protocols/common.h>
#include <services/internet/protocols/inet.h>
#include <services/bluetooth/protocols/asker_inet.h>
#include <services/bluetooth/protocols/sdp/pdu_builder.h>
#include <services/socket/netdb.h>
#include <common/sync.h>

#include <string>
#include <mutex>

namespace eka2l1::epoc::bt {
    class midman;
    class sdp_inet_protocol;

    class sdp_inet_net_database: public socket::net_database {
    private:
        sdp_inet_protocol *protocol_;
        asker_inet bt_port_asker_;
        epoc::notify_info current_query_notify_;

        void *sdp_connect_;
        bool connected_;
        pdu_builder pdu_packet_builder_;
        std::vector<char> pdu_response_buffer_;
        std::vector<char> stored_query_buffer_;

        epoc::des8 *provided_result_;
        bool store_to_temp_buffer_;

        std::mutex access_lock_;
        common::event close_done_evt_;
        bool should_notify_done_;

        void handle_connect_query(const char *record_buf, const std::uint32_t record_size);
        void handle_service_query(const char *record_buf, const std::uint32_t record_size);
        void handle_encoded_query(const char *record_buf, const std::uint32_t record_size);
        void handle_retrieve_buffer_query();
        void send_pdu_packet(const char *buf, const std::uint32_t buf_size);
        void handle_normal_query_complete(const std::uint8_t *param, const std::uint32_t param_len);
        void close_connect_handle(const bool should_wait = false);

    public:
        explicit sdp_inet_net_database(sdp_inet_protocol *protocol);
        ~sdp_inet_net_database() override;

        void query(const char *query_data, const std::uint32_t query_size, epoc::des8 *result_buffer, epoc::notify_info &complete_info) override;
        void add(const char *record_buf, const std::uint32_t record_size, epoc::notify_info &complete_info) override;
        void remove(const char *record_buf, const std::uint32_t record_size, epoc::notify_info &complete_info) override;
        void cancel() override;

        void handle_connect_done(const int status);
        void handle_send_done(const int status);
        char *prepare_read_buffer(const std::size_t suggested_size);
        void handle_new_pdu_packet(const char *buffer, const std::int64_t nread);
    };

    class sdp_inet_protocol : public socket::protocol {
    private:
        // lmao no
        midman *mid_;

    public:
        explicit sdp_inet_protocol(midman *mid, const bool oldarch)
            : socket::protocol(oldarch)
            , mid_(mid) {
        }

        midman *get_midman() {
            return mid_;
        }

        virtual std::u16string name() const override {
            return u"SDP";
        }

        std::vector<std::uint32_t> family_ids() const override {
            return { BTADDR_PROTOCOL_FAMILY_ID };
        }

        virtual std::vector<std::uint32_t> supported_ids() const override {
            return { SDP_PROTOCOL_ID };
        }

        virtual epoc::version ver() const override {
            epoc::version v;
            v.major = 0;
            v.minor = 0;
            v.build = 1;

            return v;
        }

        virtual epoc::socket::byte_order get_byte_order() const override {
            return epoc::socket::byte_order_little_endian;
        }

        virtual std::unique_ptr<epoc::socket::socket> make_socket(const std::uint32_t family_id, const std::uint32_t protocol_id, const socket::socket_type sock_type) override {
            // L2CAP will be the one handling this. This stack is currently only used for managing links.
            return nullptr;
        }

        virtual std::unique_ptr<epoc::socket::host_resolver> make_host_resolver(const std::uint32_t id, const std::uint32_t family_id) override {
            return nullptr;
        }

        virtual std::unique_ptr<epoc::socket::net_database> make_net_database(const std::uint32_t id, const std::uint32_t family_id) override {
            return std::make_unique<sdp_inet_net_database>(this);
        }
    };
}
