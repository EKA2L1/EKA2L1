/*
 * Copyright (c) 2023 EKA2L1 Team
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

#include <services/bluetooth/protocols/btmidman_inet.h>
#include <services/bluetooth/protocols/common_inet.h>
#include <services/bluetooth/protocols/utils_inet.h>

namespace eka2l1::epoc::bt {
    void midman_inet::setup_lan_discovery() {
        if (!epoc::internet::retrieve_local_ip_info(server_addr_, &local_addr_)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Can't find local LAN interface for BT netplay!");
            return;
        }

        auto loop = uvw::loop::get_default();

        local_addr_.port_ = static_cast<std::uint16_t>(LAN_DISCOVERY_PORT);
        lan_discovery_call_listener_socket_ = loop->resource<uvw::udp_handle>();

        run_task_on(loop, [this]() {
            sockaddr_in6 addr_bind;
            std::memset(&addr_bind, 0, sizeof(sockaddr_in6));
            addr_bind.sin6_family = AF_INET;
            addr_bind.sin6_port = htons(static_cast<std::uint16_t>(LAN_DISCOVERY_PORT));

            lan_discovery_call_listener_socket_->bind(*reinterpret_cast<sockaddr*>(&addr_bind));
            lan_discovery_call_listener_socket_->on<uvw::error_event>([this](const uvw::error_event &event, uvw::udp_handle &handle) {
                handle_lan_discovery_receive(nullptr, event.code(), nullptr);
            });

            lan_discovery_call_listener_socket_->on<uvw::udp_data_event>([this](const uvw::udp_data_event &event, uvw::udp_handle &handle) {
                sockaddr sender_ced = uvw::details::ip_addr(event.sender.ip.data(), event.sender.port);
                handle_lan_discovery_receive(event.data.get(), event.length, &sender_ced);
            });

            lan_discovery_call_listener_socket_->recv();
        });
    }

    void midman_inet::handle_lan_discovery_receive(const char *buf, std::int64_t nread, const sockaddr *addr) {
        if (memcmp(&(reinterpret_cast<const sockaddr_in*>(addr)->sin_addr), local_addr_.user_data_, 4) == 0) {
            return;
        }

        char exist_opcode = QUERY_OPCODE_NOTIFY_PLAYER_EXISTENCE;

        if (buf[0] == QUERY_OPCODE_NOTIFY_PLAYER_EXISTENCE) {
            add_lan_friend(addr);
        } else if (buf[0] == QUERY_OPCODE_GET_PLAYERS) {
            // Accept this discover call by sending back to the one waving at us!
            if ((buf[1] == password_.length()) && (std::memcmp(password_.data(), buf + 2, buf[1]) == 0)) {
                for (std::uint16_t i = 0; i < RETRY_LAN_DISCOVERY_TIME_MAX; i++) {
                    lan_discovery_call_listener_socket_->send(*addr, &exist_opcode, 1);
                }
            }
        }
    }

    void midman_inet::add_lan_friend(const sockaddr *addr) {
        epoc::bt::friend_info info;
        info.dvc_addr_.padding_ = 0;

        epoc::internet::host_sockaddr_to_guest_saddress(addr, info.real_addr_);
        info.real_addr_.port_ = port_;

        add_friend(info);
    }
}