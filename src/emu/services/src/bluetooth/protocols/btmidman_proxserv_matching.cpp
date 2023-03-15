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
#include <services/utils_uvw.h>
#include <config/config.h>

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

        run_task_on(loop, [matching_server_socket_copy, meta_server_addr, this]() {
            auto matching_server_socket_copy_copy = matching_server_socket_copy;

            sockaddr_in6 addr_temp;
            std::memset(&addr_temp, 0, sizeof(sockaddr_in6));
            addr_temp.sin6_family = meta_server_addr.sin6_family;

            matching_server_socket_copy_copy->bind(*reinterpret_cast<sockaddr*>(&addr_temp));

            matching_server_socket_copy_copy->on<uvw::error_event>([this](const uvw::error_event &event, uvw::tcp_handle &handle) {
                // Accept the fate and resend login
                send_login();
            });

            matching_server_socket_copy_copy->on<uvw::connect_event>([matching_server_socket_copy_copy, this](const uvw::connect_event &event, uvw::tcp_handle &handle) {
                matching_server_socket_copy_copy->on<uvw::data_event>([this](const uvw::data_event &event, uvw::tcp_handle &handle) {
                    handle_matching_server_msg(static_cast<std::int64_t>(event.length), event.data.get());
                });

                matching_server_socket_copy_copy->read();
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
            LOG_ERROR(SERVICE_BLUETOOTH, , err);
        }

        if (close_and_reset) {
            auto matching_server_socket_copy = matching_server_socket_;
            run_task_on(uvw::loop::get_default(), [matching_server_socket_copy]() {
                auto matching_server_socket_copy_copy = matching_server_socket_copy;
                matching_server_socket_copy_copy.reset();
            });
        }
    }

    void midman_inet::handle_matching_server_msg(std::int64_t nread, const char *buf) {
        if (buf[0] == QUERY_OPCODE_NOTIFY_PLAYER_EXISTENCE) {
            if (!current_active_observer_) {
                return;
            }

            hearing_timeout_timer_->stop();

            char friend_count = buf[1];
            char packet_stream_pointer = 2;

            for (char i = 0; i < friend_count; i++) {
                if (packet_stream_pointer >= nread) {
                    break;
                }

                read_and_add_friend(buf, packet_stream_pointer);
            }

            // No need to restart, it's one time thing
            on_timeout_friend_search();
        }

        return;
    }
}