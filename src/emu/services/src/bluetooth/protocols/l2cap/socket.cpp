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

#include <services/bluetooth/protocols/l2cap/l2cap.h>
#include <services/bluetooth/btmidman.h>
#include <common/log.h>

namespace eka2l1::epoc::bt {
    std::size_t l2cap_socket::get_link_count(std::uint8_t *buffer, const std::size_t avail_size) {
        LOG_TRACE(SERVICE_BLUETOOTH, "Link count stubbed with 0");
        
        if (avail_size < 4) {
            return static_cast<std::size_t>(-1);
        }

        *reinterpret_cast<std::uint32_t*>(buffer) = 0;
        return 4;
    }
    
    std::size_t l2cap_socket::get_option(const std::uint32_t option_id, const std::uint32_t option_family,
        std::uint8_t *buffer, const std::size_t avail_size) {
        if (option_family == SOL_BT_LINK_MANAGER) {
            if (pr_->is_oldarch()) {
                switch (option_id) {
                case l2cap_socket_oldarch_link_count:
                    return get_link_count(buffer, avail_size);

                default:
                    LOG_WARN(SERVICE_BLUETOOTH, "Unhandled option {} in link manager option family", option_id);
                    return 0;
                }
            } else {
                switch (option_id) {
                case l2cap_socket_link_count:
                    return get_link_count(buffer, avail_size);

                default:
                    LOG_WARN(SERVICE_BLUETOOTH, "Unhandled option {} in link manager option family", option_id);
                    return 0;
                }
            }
        }

        LOG_ERROR(SERVICE_BLUETOOTH, "Unhandled bluetooth option family {} (id {})", option_family, option_id);
        return 0;
    }

    bool l2cap_socket::set_option(const std::uint32_t option_id, const std::uint32_t option_family,
        std::uint8_t *buffer, const std::size_t avail_size) {
        LOG_WARN(SERVICE_BLUETOOTH, "Set option is not supported yet with L2CAP socket");
        return false;
    }

    std::int32_t l2cap_protocol::message_size() const {
        // TODO: Proper size
        return epoc::socket::SOCKET_MESSAGE_SIZE_NO_LIMIT;
    }
}