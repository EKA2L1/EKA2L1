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

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <uvw.hpp>

typedef struct uv_timer_s uv_timer_t;
typedef struct uv_buf_t uv_buf_t;

namespace eka2l1::epoc::bt {
    static constexpr std::uint32_t TIMEOUT_HEARING_STRANGER_MS = 2000;
    static constexpr std::uint16_t CENTRAL_SERVER_STANDARD_PORT = 27138;
    static constexpr std::uint16_t HARBOUR_PORT = 35689;
    static constexpr std::uint16_t LAN_DISCOVERY_PORT = 35690;
    static constexpr std::uint32_t TIMEOUT_HEARING_STRANGER_LAN_MS = 400;
    static constexpr std::uint16_t RETRY_LAN_DISCOVERY_TIME_MAX = 5;

    struct friend_info {
        epoc::socket::saddress real_addr_;
        device_address dvc_addr_;
        bool refreshed_ = false;
    };

    enum friend_update_error : std::uint64_t {
        FRIEND_UPDATE_ERROR_INVALID_PORT_NUMBER = 1ULL << 32,
        FRIEND_UPDATE_ERROR_INVALID_ADDR = 2ULL << 32,
        FRIEND_UPDATE_ERROR_MASK = 0xFFFFFFFF00000000ULL,
        FRIEND_UPDATE_INDEX_FAULT_MASK = 0x00000000FFFFFFFFULL
    };

    enum discovery_mode {
        DISCOVERY_MODE_OFF = 0,
        DISCOVERY_MODE_DIRECT_IP = 1,
        DISCOVERY_MODE_LAN = 2,
        DISCOVERY_MODE_PROXY_SERVER = 3
    };

    struct inet_stranger_call_observer {
    public:
        virtual void on_stranger_call(epoc::socket::saddress &addr, std::uint32_t index_in_list) = 0;
        virtual void on_no_more_strangers() = 0;
    };

    class midman_inet: public midman {
    private:
        std::map<device_address, std::uint32_t> friend_device_address_mapping_;
 
        device_address random_device_addr_;     // Hope it will never collide with what friends you want to add

        std::vector<friend_info> friends_;
        common::bitmap_allocator allocated_ports_;
        std::array<std::uint16_t, MAX_PORT> port_refs_;
        std::uint32_t port_offset_;
        bool enable_upnp_;

        bool friend_info_cached_;

        std::shared_ptr<uvw::udp_handle> lan_discovery_call_listener_socket_;
        std::shared_ptr<uvw::tcp_handle> matching_server_socket_;

        std::shared_ptr<uvw::udp_handle> bluetooth_queries_server_socket_;
        std::shared_ptr<uvw::timer_handle> hearing_timeout_timer_;

        int port_;
        int retried_lan_discovery_times_;

        std::mutex friends_lock_;
        asker_inet device_addr_asker_;

        inet_stranger_call_observer *current_active_observer_;
        std::vector<inet_stranger_call_observer*> pending_observers_;

        std::string password_;
        discovery_mode discovery_mode_;

        epoc::socket::saddress server_addr_;
        epoc::socket::saddress local_addr_;

        std::shared_ptr<libuv::task> send_strangers_call_task_;
        std::shared_ptr<libuv::task> reset_timeout_timer_task_;

        std::uint32_t asker_counter_;

        void send_call_for_strangers();

        // LAN
        void setup_lan_discovery();
        void add_lan_friend(const sockaddr *replier);
        void handle_lan_discovery_receive(const char *buf, std::int64_t nread, const sockaddr *addr);

        // Proxy server
        void setup_proxy_server_discovery(const std::string &base_server);

        // Server handler
        void handle_matching_server_msg(std::int64_t nread, const char *buf_ptr);
        void send_login();
        void send_logout(const bool close_and_reset = true);
        void read_and_add_friend(const char *buf, char &buf_pointer);
        void add_friend(epoc::bt::friend_info &info);
        void on_timeout_friend_search();

        void reset_friend_timeout_timer();
        bool should_upnp_apply_to_port();

        void refresh_friend_info_async_impl(std::uint32_t pos, std::function<void()> callback);

    public:
        explicit midman_inet(const config::state &conf);
        ~midman_inet() override;

        std::uint32_t get_host_port(const std::uint16_t virtual_port);
        void ref_and_public_port(const std::uint16_t virtual_port);
        void close_port(const std::uint16_t virtual_port);
        void ref_port(const std::uint16_t virtual_port);
        std::uint16_t get_free_port();

        std::vector<std::uint32_t> get_friend_index_with_address(epoc::socket::saddress &addr);
        bool get_friend_device_address(const std::uint32_t index, device_address &result);
        void handle_queries_request(const sockaddr *addr, const char *buf, std::int64_t nread);

        bool get_friend_address(const std::uint32_t index, epoc::socket::saddress &addr);
        bool get_friend_address(const device_address &friend_virt_addr, epoc::socket::saddress &addr);
        void get_friend_address_async(const device_address &friend_virt_addr, std::function<void(epoc::socket::saddress*)> callback);

        void add_device_address_mapping(const std::uint32_t index, const device_address &addr);
        void refresh_friend_infos();
        void refresh_friend_infos_async(std::function<void()> callback);

        void update_friend_list(const std::vector<config::friend_address> &addrs, std::vector<std::uint64_t> &invalid_address_indicies);
        void add_or_update_friend(const epoc::socket::saddress &addr);

        void begin_hearing_stranger_call(inet_stranger_call_observer *observer);
        void unregister_stranger_call_observer(inet_stranger_call_observer *observer);

        const int get_server_port() const {
            return port_;
        }

        const device_address local_device_address() const {
            return random_device_addr_;
        }

        void set_friend_info_cached() {
            friend_info_cached_ = true;
        }

        void clear_friend_info_cached() {
            friend_info_cached_ = false;
        }
 
        midman_type type() const override {
            return MIDMAN_INET_BT;
        }

        discovery_mode get_discovery_mode() const {
            return discovery_mode_;
        }

        std::uint32_t get_port_offset() const {
            return port_offset_;
        }

        std::uint32_t new_asker_id() {
            return ++asker_counter_;
        }
    };
}
