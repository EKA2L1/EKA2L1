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

#include <services/internet/protocols/inet.h>
#include <common/sync.h>

namespace eka2l1::epoc::bt {
    struct asker_inet {
    private:
        void *bt_asker_;
        void *bt_asker_retry_timer_;

        common::event ask_routed_port_wait_evt_;

    public:
        using response_callback = std::function<void(const char *response, const std::int64_t size)>;
        using port_ask_done_callback = std::function<void(std::int64_t port_result)>;

    public:
        explicit asker_inet();
        ~asker_inet();

        void send_request_with_retries(const epoc::socket::saddress &addr, char *request, const std::size_t request_size,
            response_callback response_cb, const bool request_dynamically_allocated = false);

        std::uint32_t ask_for_routed_port(const std::uint16_t virtual_port, const epoc::socket::saddress &dev_addr);
        void ask_for_routed_port_async(const std::uint16_t virtual_port, const epoc::socket::saddress &dev_addr, port_ask_done_callback cb);
    };
}
