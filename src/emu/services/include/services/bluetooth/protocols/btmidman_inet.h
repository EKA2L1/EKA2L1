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
#include <services/internet/protocols/inet.h>
#include <common/allocator.h>
#include <common/sync.h>

extern "C" {
#include <uv.h>
}

#include <functional>
#include <map>
#include <mutex>
#include <vector>

namespace eka2l1::epoc::bt {
    class midman_inet: public midman {
    public:
        static constexpr std::uint32_t MAX_INET_DEVICE_AROUND = 4;

    private:
        std::map<std::uint16_t, std::uint32_t> port_map_;

        // Address in IPv6 with the query port.
        // The query port can give out information at runtime about:
        // - The name of the host's device
        // - Port mapping (from guest port to real port)
        std::array<internet::sinet6_address, MAX_INET_DEVICE_AROUND> hosters_;
        common::bitmap_allocator allocated_ports_;

        uv_udp_t *virt_bt_info_server_;

        std::vector<char> server_recv_buf_;
        int port_;

        std::mutex hosters_lock_;

    public:
        explicit midman_inet(const int server_port);
        ~midman_inet();

        std::uint32_t lookup_host_port(const std::uint16_t virtual_port);
        void add_host_port(const std::uint16_t virtual_port, const std::uint32_t host_port);
        void close_port(const std::uint16_t virtual_port);
        std::uint16_t get_free_port();

        void prepare_server_recv_buffer(uv_buf_t *buf, const std::size_t suggested_size);
        void handle_server_request(const sockaddr *requester, const uv_buf_t *buf, ssize_t nread);

        bool get_friend_address(const std::uint32_t index, internet::sinet6_address &addr);
    };
}