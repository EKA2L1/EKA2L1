/*
 * Copyright (c) 2021 EKA2L1 Team
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

#include <services/internet/protocols/overall.h>
#include <services/internet/protocols/udp/udp.h>
#include <services/socket/server.h>

#include <common/log.h>

namespace eka2l1::epoc::internet {
    void add_internet_stack_protocols(socket_server *sock, const bool oldarch) {
        std::unique_ptr<epoc::socket::protocol> udp_pr = std::make_unique<udp_protocol>(oldarch);

        if (!sock->add_protocol(udp_pr)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Failed to add INET UDP protocol");
        }
    }
}
