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

#include <services/bluetooth/protocols/btlink/btlink_inet.h>
#include <services/bluetooth/protocols/l2cap/l2cap_inet.h>
#include <services/bluetooth/protocols/rfcomm/rfcomm_inet.h>
#include <services/bluetooth/protocols/sdp/sdp_inet.h>
#include <services/bluetooth/protocols/overall.h>
#include <services/internet/protocols/inet.h>
#include <services/socket/server.h>

#include <common/log.h>

namespace eka2l1::epoc::bt {
    void add_bluetooth_stack_protocols(socket_server *sock, epoc::bt::midman *mm, const bool oldarch) {
        internet::inet_bridged_protocol *protocol = reinterpret_cast<internet::inet_bridged_protocol*>(sock->find_protocol(internet::INET6_ADDRESS_FAMILY, internet::INET_TCP_PROTOCOL_ID));
        protocol->initialize_looper();  // NOTE: Real non-internet bluetooth stack will not need this.

        std::unique_ptr<epoc::socket::protocol> btlink_pr = std::make_unique<btlink_inet_protocol>(mm, oldarch);
        std::unique_ptr<epoc::socket::protocol> l2cap_pr = std::make_unique<l2cap_inet_protocol>(mm, protocol, oldarch);
        std::unique_ptr<epoc::socket::protocol> rfcomm_pr = std::make_unique<rfcomm_inet_protocol>(mm, protocol, oldarch);
        std::unique_ptr<epoc::socket::protocol> sdp_pr = std::make_unique<sdp_inet_protocol>(mm, oldarch);

        if (!sock->add_protocol(btlink_pr)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Failed to add BTLink protocol");
        }

        if (!sock->add_protocol(l2cap_pr)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Failed to add L2CAP protocol");
        }

        if (!sock->add_protocol(rfcomm_pr)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Failed to add L2CAP protocol");
        }

        if (!sock->add_protocol(sdp_pr)) {
            LOG_ERROR(SERVICE_BLUETOOTH, "Failed to add SDP protocol");
        }
    }
}
