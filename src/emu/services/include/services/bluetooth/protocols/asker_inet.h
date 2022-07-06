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

extern "C" {
#include <uv.h>
}

namespace eka2l1::epoc::bt {
    struct asker_inet {
    private:
        uv_udp_t *bt_asker_;
        uv_timer_t *bt_asker_retry_timer_;

    public:
        using response_callback = std::function<void(const char *response, const ssize_t size)>;

    public:
        explicit asker_inet();
        ~asker_inet();

        void send_request_with_retries(const internet::sinet6_address &addr, char *request, const std::size_t request_size,
            response_callback response_cb);
    };
}