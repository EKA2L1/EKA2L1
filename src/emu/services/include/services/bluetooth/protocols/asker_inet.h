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

#include <services/bluetooth/protocols/common.h>
#include <services/internet/protocols/inet.h>
#include <common/sync.h>

#include <memory>
#include <optional>

#include <uvw.hpp>

namespace eka2l1::epoc::bt {
    enum {
        BT_COMM_INET_ERROR_SEND_FAILED = -1,
        BT_COMM_INET_ERROR_RECV_FAILED = -2,
        BT_COMM_INET_INVALID_ADDR = -3
    };

    static constexpr std::uint32_t SEND_TIMEOUT_RETRY = 500;

    struct asker_inet {
    public:
        using response_callback = std::function<int(const char *response, const std::int64_t size)>;
        using device_address_get_done_callback = std::function<void(device_address *addr)>;
        using name_get_done_callback = std::function<void(const char *name, const char length)>;
        using port_ask_done_callback = std::function<void(std::int64_t port_result)>;

    private:
        int retry_times_;
        char *buf_;
        std::uint32_t buf_size_;
        bool dynamically_allocated_;

        response_callback callback_;
        sockaddr_in6 dest_;

        std::shared_ptr<uvw::udp_handle> asker_;
        std::shared_ptr<uvw::timer_handle> asker_retry_timer_;

        common::event request_done_evt_;

        bool in_transfer_data_callback_;

        void keep_sending_data();
        void handle_request_failure();
        void listen_to_data();

    public:
        explicit asker_inet();
        ~asker_inet();

        void send_request_with_retries(const epoc::socket::saddress &addr, char *request, const std::size_t request_size,
            response_callback response_cb, const bool request_dynamically_allocated = false, const bool sync = false);
        
        bool check_is_real_port_mapped(const epoc::socket::saddress &addr, const std::uint32_t real_port);
        std::optional<device_address> get_device_address(const epoc::socket::saddress &dest_friend);
        void get_device_address_async(const epoc::socket::saddress &dest_friend, device_address_get_done_callback callback);
        void get_device_name_async(const epoc::socket::saddress &dest_friend, name_get_done_callback callback);

        std::uint32_t ask_for_routed_port(const std::uint16_t virtual_port, const epoc::socket::saddress &dev_addr);
        void ask_for_routed_port_async(const std::uint16_t virtual_port, const epoc::socket::saddress &dev_addr, port_ask_done_callback cb);
    };
}
