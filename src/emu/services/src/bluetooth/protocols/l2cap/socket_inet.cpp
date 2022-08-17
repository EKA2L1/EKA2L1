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
#include <services/bluetooth/protocols/l2cap/l2cap_inet.h>
#include <services/internet/protocols/inet.h>

namespace eka2l1::epoc::bt {
    std::size_t l2cap_inet_socket::get_link_count(std::uint8_t *buffer, const std::size_t avail_size) {
        LOG_TRACE(SERVICE_BLUETOOTH, "Link count stubbed with 0");

        if (avail_size < 4) {
            return static_cast<std::size_t>(-1);
        }

        *reinterpret_cast<std::uint32_t *>(buffer) = 0;
        return 4;
    }

    std::size_t l2cap_inet_socket::get_option(const std::uint32_t option_id, const std::uint32_t option_family,
        std::uint8_t *buffer, const std::size_t avail_size) {
        if (option_family == SOL_BT_LINK_MANAGER) {
            if (protocol_->is_oldarch()) {
                switch (option_id) {
                case btlink_socket_oldarch_link_count:
                    return get_link_count(buffer, avail_size);

                default:
                    LOG_WARN(SERVICE_BLUETOOTH, "Unhandled option {} in link manager option family", option_id);
                    return 0;
                }
            } else {
                switch (option_id) {
                case btlink_socket_link_count:
                    return get_link_count(buffer, avail_size);

                default:
                    LOG_WARN(SERVICE_BLUETOOTH, "Unhandled option {} in link manager option family", option_id);
                    return 0;
                }
            }
        } else if (option_family == SOL_BT_L2CAP) {
            switch (option_id) {
            // All is 1024 - 4. See TBTL2CAPOptions comments
            case l2cap_socket_get_inbound_mtu_for_best_perf:
            case l2cap_socket_get_outbound_mtu_for_best_perf:
                *reinterpret_cast<std::uint32_t *>(buffer) = 1020;
                return 4;

            default:
                LOG_WARN(SERVICE_BLUETOOTH, "Unhandled option {} in L2CAP option family", option_id);
                return 0;
            }
        }

        return socket::get_option(option_id, option_family, buffer, avail_size);
    }

    bool l2cap_inet_socket::set_option(const std::uint32_t option_id, const std::uint32_t option_family,
        std::uint8_t *buffer, const std::size_t avail_size) {
        return socket::set_option(option_id, option_family, buffer, avail_size);
    }

    void l2cap_inet_socket::receive(std::uint8_t *data, const std::uint32_t data_size, std::uint32_t *sent_size, epoc::socket::saddress *addr,
        std::uint32_t flags, epoc::notify_info &complete_info, epoc::socket::receive_done_callback done_callback) {
        btinet_socket::receive(data, data_size, sent_size, addr, flags | epoc::socket::SOCKET_FLAG_DONT_WAIT_FULL, complete_info,
            done_callback);
    }

    std::int32_t l2cap_inet_protocol::message_size() const {
        // TODO: Proper size
        return epoc::socket::SOCKET_MESSAGE_SIZE_NO_LIMIT;
    }

    std::unique_ptr<epoc::socket::socket> l2cap_inet_protocol::make_socket(const std::uint32_t family_id, const std::uint32_t protocol_id, const socket::socket_type sock_type) {
        std::unique_ptr<epoc::socket::socket> net_socket = inet_protocol_->make_socket(internet::INET6_ADDRESS_FAMILY, internet::INET_TCP_PROTOCOL_ID, socket::socket_type_stream);
        return std::make_unique<l2cap_inet_socket>(this, net_socket);
    }
}