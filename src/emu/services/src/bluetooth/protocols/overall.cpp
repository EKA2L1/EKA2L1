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

#include <services/bluetooth/protocols/overall.h>
#include <services/bluetooth/protocols/btlink/btlink.h>
#include <services/bluetooth/protocols/l2cap/l2cap.h>
#include <services/socket/server.h>

#include <common/log.h>

namespace eka2l1::epoc::bt {
    void add_bluetooth_stack_protocols(socket_server *sock, epoc::bt::midman *mm, const bool oldarch) {
        std::unique_ptr<epoc::socket::protocol> btlink_pr = std::make_unique<btlink_protocol>(mm, oldarch);
        std::unique_ptr<epoc::socket::protocol> l2cap_pr = std::make_unique<l2cap_protocol>(mm, oldarch);

        if (!sock->add_protocol(btlink_pr)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Failed to add BTLink protocol");
        }

        if (!sock->add_protocol(l2cap_pr)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Failed to add L2CAP protocol");
        }
    }
}
