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
#include <mutex>
#include <vector>

typedef struct uv_timer_s uv_timer_t;
typedef struct uv_buf_t uv_buf_t;

namespace eka2l1::epoc::bt {
    struct friend_info {
        epoc::socket::saddress real_addr_;
        device_address dvc_addr_;
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
        DISCOVERY_MODE_LOCAL_LAN = 2,
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

        void *virt_bt_info_server_;
        void *virt_server_socket_;
        void *hearing_timeout_timer_;

        std::vector<char> server_recv_buf_;
        int port_;

        std::mutex friends_lock_;
        asker_inet device_addr_asker_;

        inet_stranger_call_observer *current_active_observer_;
        std::vector<inet_stranger_call_observer*> pending_observers_;

        std::string password_;
        discovery_mode discovery_mode_;

        epoc::socket::saddress server_addr_;
        epoc::socket::saddress local_addr_;

        void send_call_for_strangers();
        void handle_meta_server_msg(std::int64_t nread, const uv_buf_t *buf_ptr);
        void send_login();
        
        static void reset_friend_timeout_timer(uv_timer_t *timer);
        bool should_upnp_apply_to_port();

    public:
        explicit midman_inet(const config::state &conf);
        ~midman_inet() override;

        std::uint32_t lookup_host_port(const std::uint16_t virtual_port);
        void add_host_port(const std::uint16_t virtual_port);
        void close_port(const std::uint16_t virtual_port);
        void ref_port(const std::uint16_t virtual_port);
        std::uint16_t get_free_port();

        std::vector<std::uint32_t> get_friend_index_with_address(epoc::socket::saddress &addr);
        bool get_friend_device_address(const std::uint32_t index, device_address &result);

        void prepare_server_recv_buffer(void *buf_trans, const std::size_t suggested_size);
        void handle_server_request(const sockaddr *requester, const void *buf, std::int64_t nread);

        bool get_friend_address(const std::uint32_t index, epoc::socket::saddress &addr);
        bool get_friend_address(const device_address &friend_virt_addr, epoc::socket::saddress &addr);

        void add_device_address_mapping(const std::uint32_t index, const device_address &addr);
        void refresh_friend_infos();

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

        static void send_logout(void *hh, bool close_and_reset = false);
    };
}
