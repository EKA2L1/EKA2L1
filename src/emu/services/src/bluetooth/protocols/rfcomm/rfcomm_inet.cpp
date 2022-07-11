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

#include <common/log.h>
#include <services/bluetooth/btmidman.h>
#include <services/bluetooth/protocols/rfcomm/rfcomm_inet.h>
#include <services/bluetooth/protocols/btmidman_inet.h>
#include <services/internet/protocols/inet.h>

namespace eka2l1::epoc::bt {
    std::size_t rfcomm_inet_socket::get_option(const std::uint32_t option_id, const std::uint32_t option_family,
        std::uint8_t *buffer, const std::size_t avail_size) {
        midman_inet *midman = reinterpret_cast<midman_inet*>(protocol_->get_midman());

        if (option_family == SOL_BT_RFCOMM) {
            switch (option_id) {
            case rfcomm_opt_get_available_server_channel:
                *reinterpret_cast<std::uint32_t*>(buffer) = midman->get_free_port();
                return 4;

            default:
                LOG_WARN(SERVICE_BLUETOOTH, "Unhandled option {} in RFCOMM option family", option_id);
                return 0;
            }
        }

        return socket::get_option(option_id, option_family, buffer, avail_size);
    }

    std::int32_t rfcomm_inet_protocol::message_size() const {
        // TODO: Proper size
        return epoc::socket::SOCKET_MESSAGE_SIZE_NO_LIMIT;
    }

    std::unique_ptr<epoc::socket::socket> rfcomm_inet_protocol::make_socket(const std::uint32_t family_id, const std::uint32_t protocol_id, const socket::socket_type sock_type) {
        std::unique_ptr<epoc::socket::socket> net_socket = inet_protocol_->make_socket(internet::INET6_ADDRESS_FAMILY, internet::INET_TCP_PROTOCOL_ID, socket::socket_type_stream);
        return std::make_unique<rfcomm_inet_socket>(this, net_socket);
    }
}