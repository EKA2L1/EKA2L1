/*
 * Copyright (c) 2022 EKA2L1 Team
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

#include <services/bluetooth/btmidman.h>
#include <services/bluetooth/protocols/common.h>
#include <services/internet/protocols/inet.h>
#include <services/bluetooth/protocols/asker_inet.h>
#include <common/allocator.h>
#include <common/sync.h>
#include <config/config.h>

extern "C" {
#include <uv.h>
}

#include <functional>
#include <map>
#include <mutex>
#include <vector>

namespace eka2l1::epoc::bt {
    struct friend_info {
        epoc::socket::saddress real_addr_;
        device_address dvc_addr_;
    };

    enum friend_update_error : std::uint64_t {
        FRIEND_UPDATE_ERROR_INVALID_PORT_NUMBER = 1ULL << 32,
        FRIEND_UPDATE_ERROR_INVALID_ADDR = 2ULL << 32
    };

    class midman_inet: public midman {
    public:
        static constexpr std::uint32_t MAX_INET_DEVICE_AROUND = 10;

    private:
        std::map<std::uint16_t, std::uint32_t> port_map_;
        std::map<device_address, std::uint32_t> friend_device_address_mapping_;
 
        device_address random_device_addr_;     // Hope it will never collide with what friends you want to add

        std::array<friend_info, MAX_INET_DEVICE_AROUND> friends_;
        common::bitmap_allocator allocated_ports_;
        bool friend_info_cached_;

        uv_udp_t *virt_bt_info_server_;
        std::vector<char> server_recv_buf_;
        int port_;

        std::mutex friends_lock_;
        asker_inet device_addr_asker_;

    public:
        explicit midman_inet(const config::state &conf);
        ~midman_inet();

        std::uint32_t lookup_host_port(const std::uint16_t virtual_port);
        void add_host_port(const std::uint16_t virtual_port, const std::uint32_t host_port);
        void close_port(const std::uint16_t virtual_port);
        std::uint16_t get_free_port();

        void prepare_server_recv_buffer(uv_buf_t *buf, const std::size_t suggested_size);
        void handle_server_request(const sockaddr *requester, const uv_buf_t *buf, ssize_t nread);

        bool get_friend_address(const std::uint32_t index, epoc::socket::saddress &addr);
        bool get_friend_address(const device_address &friend_virt_addr, epoc::socket::saddress &addr);

        void add_device_address_mapping(const std::uint32_t index, const device_address &addr);
        void refresh_friend_infos();

        void update_friend_list(const std::vector<config::friend_address> &addrs, std::vector<std::uint64_t> &invalid_address_indicies);

        const int get_server_port() const {
            return port_;
        }

        const device_address local_device_address() const {
            return random_device_addr_;
        }

        void set_friend_info_cached() {
            friend_info_cached_ = true;
        }
        
        midman_type type() const override {
            return MIDMAN_INET_BT;
        }
    };
}