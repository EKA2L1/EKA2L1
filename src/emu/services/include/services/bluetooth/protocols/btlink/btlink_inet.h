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
#include <services/bluetooth/protocols/asker_inet.h>
#include <services/bluetooth/protocols/btmidman_inet.h>
#include <services/internet/protocols/inet.h>
#include <services/socket/protocol.h>

#include <atomic>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <tuple>

namespace eka2l1::epoc::bt {
    class midman;
    class btlink_inet_protocol;

    class btlink_inet_host_resolver : public socket::host_resolver, public inet_stranger_call_observer {
    private:
        btlink_inet_protocol *papa_;

        std::uint32_t current_friend_;
        bool need_name_;

        bool no_more_friends_;
        bool in_async_searching_;

        std::set<std::uint32_t> searched_list_async_;
        std::queue<std::pair<epoc::socket::saddress, std::uint32_t>> new_friends_wait_;

        asker_inet friend_querier_;

        epoc::notify_info friend_retrieve_info_;
        epoc::socket::name_entry *friend_name_entry_;

        std::atomic<bool> in_completion_;
        int delay_emu_evt_;

        std::mutex lock_;

        void next_impl();
        void report_search_end_to_client();
        void search_next_friend_and_report_to_client(epoc::socket::saddress &addr, std::uint32_t index);

    public:
        explicit btlink_inet_host_resolver(btlink_inet_protocol *papa);
        ~btlink_inet_host_resolver();

        std::u16string host_name() const override;
        bool host_name(const std::u16string &name) override;

        void get_by_address(epoc::socket::saddress &addr, epoc::socket::name_entry *result, epoc::notify_info &info) override;
        void get_by_name(epoc::socket::name_entry *result, epoc::notify_info &info) override;
        void next(epoc::socket::name_entry *result, epoc::notify_info &info) override;

        void complete_background_find_device_delay_emulation();
        
        void on_stranger_call(epoc::socket::saddress &addr, std::uint32_t index_in_list) override;
        void on_no_more_strangers() override;
    };

    class btlink_inet_protocol : public socket::protocol {
    private:
        // lmao no
        midman *mid_;

    public:
        explicit btlink_inet_protocol(midman *mid, const bool oldarch)
            : socket::protocol(oldarch)
            , mid_(mid) {
        }

        midman *get_midman() {
            return mid_;
        }

        virtual std::u16string name() const override {
            return u"BTLinkManager";
        }

        std::vector<std::uint32_t> family_ids() const override {
            return { BTADDR_PROTOCOL_FAMILY_ID };
        }

        virtual std::vector<std::uint32_t> supported_ids() const override {
            return { BTLINK_MANAGER_PROTOCOL_ID };
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

        virtual std::unique_ptr<epoc::socket::socket> make_empty_base_link_socket() {
            return nullptr;
        }

        virtual std::unique_ptr<epoc::socket::net_database> make_net_database(const std::uint32_t id, const std::uint32_t family_id) override {
            return nullptr;
        }

        virtual std::unique_ptr<epoc::socket::host_resolver> make_host_resolver(const std::uint32_t id, const std::uint32_t family_id) override {
            return std::make_unique<btlink_inet_host_resolver>(this);
        }
    };
}
