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
#include <config/config.h>
#include <common/log.h>

namespace eka2l1::epoc::bt {
    void midman_inet::setup_proxy_server_discovery(const std::string &base_server) {
        addrinfo *result_info = nullptr;
        addrinfo hint_info;
        std::memset(&hint_info, 0, sizeof(addrinfo));

        hint_info.ai_family = AF_UNSPEC;
        hint_info.ai_socktype = SOCK_STREAM;
        hint_info.ai_protocol = IPPROTO_TCP;

        addrinfo *ideal_result_info = nullptr;

        int result_code = getaddrinfo(base_server.c_str(), nullptr, &hint_info, &result_info);
        if (result_code != 0) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Can't resolve central server address for discovery!");

            freeaddrinfo(result_info);
            return;
        } else {
            ideal_result_info = result_info;
            while (ideal_result_info != nullptr) {
                if ((ideal_result_info->ai_family == AF_INET6) && ((ideal_result_info->ai_flags & AI_V4MAPPED) == 0)) {
                    break;
                }

                ideal_result_info = ideal_result_info->ai_next;
            }

            if (!ideal_result_info) {
                ideal_result_info = result_info;
            }
        }

        epoc::socket::name_entry entry;
        epoc::internet::addrinfo_to_name_entry(entry, ideal_result_info);

        server_addr_ = entry.addr_;
        server_addr_.port_ = CENTRAL_SERVER_STANDARD_PORT;

        auto loop = uvw::loop::get_default();

        // Send a login package
        matching_server_socket_ = loop->resource<uvw::tcp_handle>();

        sockaddr_in6 meta_server_addr;
        std::memcpy(&meta_server_addr, ideal_result_info->ai_addr, sizeof(sockaddr_in6));
        meta_server_addr.sin6_port = htons(CENTRAL_SERVER_STANDARD_PORT);

        auto matching_server_socket_copy = matching_server_socket_;

        libuv::default_looper->one_shot([matching_server_socket_copy, meta_server_addr, this]() {
            auto matching_server_socket_copy_copy = matching_server_socket_copy;

            sockaddr_in6 addr_temp;
            std::memset(&addr_temp, 0, sizeof(sockaddr_in6));
            addr_temp.sin6_family = meta_server_addr.sin6_family;

            matching_server_socket_copy_copy->bind(*reinterpret_cast<sockaddr*>(&addr_temp));

            matching_server_socket_copy_copy->on<uvw::error_event>([](const uvw::error_event &event, uvw::tcp_handle &handle) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Error on the central Bluetooth Netplay server socket! Libuv error code={}", event.code());
            });

            matching_server_socket_copy_copy->on<uvw::connect_event>([matching_server_socket_copy_copy, this](const uvw::connect_event &event, uvw::tcp_handle &handle) {
                matching_server_socket_copy_copy->on<uvw::data_event>([this](const uvw::data_event &event, uvw::tcp_handle &handle) {
                    handle_matching_server_msg(static_cast<std::int64_t>(event.length), event.data.get());
                });

                matching_server_socket_copy_copy->read();

                // The server puts us in the room named by our password, so it has to
                // hear the login before it can answer any player query.
                send_login();
            });

            int err = matching_server_socket_copy_copy->connect(*reinterpret_cast<const sockaddr *>(&meta_server_addr));

            if (err < 0) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Fail to connect to central Bluetooth Netplay server! Libuv's error code {}", err);
            }
        });

        freeaddrinfo(result_info);
    }

    void midman_inet::send_login() {
        std::string login_package;
        login_package.push_back(static_cast<char>(QUERY_OPCODE_SERVER_LOGIN));
        login_package.push_back(static_cast<char>(password_.length()));
        login_package.insert(login_package.end(), password_.begin(), password_.end());

        matching_server_socket_->on<uvw::error_event>([this](const uvw::error_event &event, uvw::tcp_handle &handle) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Fail to send login request to server! Libuv's error code {}", event.code());
        });
        
        int err = matching_server_socket_->write(login_package.data(), login_package.size());
        
        if (err < 0) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Fail to send login request to server! Libuv's error code {}", err);
        }
    }

    void midman_inet::send_logout(const bool close_and_reset) {
        if (!matching_server_socket_) {
            return;
        }

        char package = QUERY_OPCODE_SERVER_LOGOUT;

        matching_server_socket_->on<uvw::error_event>([this](const uvw::error_event &event, uvw::tcp_handle &handle) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Fail to send logout request to server! Libuv's error code {}", event.code());
        });
        
        int err = matching_server_socket_->write(&package, 1);

        if (err < 0) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Fail to send logout request to server! Libuv's error code {}", err);
        }

        if (close_and_reset) {
            auto matching_server_socket_copy = matching_server_socket_;
            libuv::default_looper->one_shot([matching_server_socket_copy]() {
            });
        }
    }

    void midman_inet::handle_matching_server_msg(std::int64_t nread, const char *buf) {
        if (!buf || (nread <= 0)) {
            return;
        }

        // Opcode, player count, then one entry per player: a type byte, an address
        // of at most 16 bytes and an optional port. Anything past that is not a
        // message this protocol can produce, so stop reassembling instead of
        // growing the buffer on whatever the socket keeps handing us.
        static constexpr std::size_t MAX_MATCHING_SERVER_MESSAGE_SIZE = 2 + 255 * (1 + 16 + 2);

        matching_server_receive_buffer_.insert(matching_server_receive_buffer_.end(), buf, buf + nread);
        if (matching_server_receive_buffer_.size() > MAX_MATCHING_SERVER_MESSAGE_SIZE) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Matching server sent an oversized reply");
            matching_server_receive_buffer_.clear();
            return;
        }

        while (matching_server_receive_buffer_.size() >= 2) {
            const std::uint8_t opcode = static_cast<std::uint8_t>(matching_server_receive_buffer_[0]);
            if (opcode == QUERY_OPCODE_SERVER_PORT_EXTENSION) {
                if (static_cast<std::uint8_t>(matching_server_receive_buffer_[1]) >= 1) {
                    const std::uint16_t advertised_port = static_cast<std::uint16_t>(port_);
                    char package[] = {
                        static_cast<char>(QUERY_OPCODE_SERVER_PORT_EXTENSION),
                        static_cast<char>(advertised_port >> 8),
                        static_cast<char>(advertised_port & 0xFF)
                    };
                    matching_server_socket_->write(package, sizeof(package));
                }
                matching_server_receive_buffer_.erase(matching_server_receive_buffer_.begin(),
                    matching_server_receive_buffer_.begin() + 2);
                continue;
            }

            if (opcode != QUERY_OPCODE_NOTIFY_PLAYER_EXISTENCE) {
                LOG_ERROR(SERVICE_BLUETOOTH, "Matching server sent unknown opcode {}", opcode);
                matching_server_receive_buffer_.clear();
                return;
            }

            const std::uint8_t friend_count = static_cast<std::uint8_t>(matching_server_receive_buffer_[1]);
            std::size_t packet_size = 2;
            bool complete = true;
            for (std::uint8_t i = 0; i < friend_count; i++) {
                if (packet_size >= matching_server_receive_buffer_.size()) {
                    complete = false;
                    break;
                }

                const std::uint8_t address_type = static_cast<std::uint8_t>(matching_server_receive_buffer_[packet_size]);
                if (address_type > 3) {
                    LOG_ERROR(SERVICE_BLUETOOTH, "Player list from the matching server has invalid address type {}", address_type);
                    matching_server_receive_buffer_.clear();
                    return;
                }

                const bool is_ipv4 = (address_type == 1) || (address_type == 2);
                const bool has_port = (address_type == 2) || (address_type == 3);
                packet_size += 1 + (is_ipv4 ? 4 : 16) + (has_port ? 2 : 0);
                if (packet_size > matching_server_receive_buffer_.size()) {
                    complete = false;
                    break;
                }
            }

            if (!complete) {
                return;
            }

            if (!current_active_observer_) {
                matching_server_receive_buffer_.erase(matching_server_receive_buffer_.begin(),
                    matching_server_receive_buffer_.begin() + packet_size);
                continue;
            }

            hearing_timeout_timer_->stop();

            std::int64_t packet_stream_pointer = 2;

            for (std::uint8_t i = 0; i < friend_count; i++) {
                read_and_add_friend(matching_server_receive_buffer_.data(), packet_size, packet_stream_pointer);
            }

            matching_server_receive_buffer_.erase(matching_server_receive_buffer_.begin(),
                matching_server_receive_buffer_.begin() + packet_size);

            // No need to restart, it's one time thing
            on_timeout_friend_search();
        }
    }
}