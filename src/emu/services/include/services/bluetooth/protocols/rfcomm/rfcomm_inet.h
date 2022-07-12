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

#pragma once

#include <services/bluetooth/protocols/btlink/btlink_inet.h>
#include <services/bluetooth/protocols/base_inet.h>
#include <services/socket/socket.h>

namespace eka2l1::epoc::internet {
    class inet_bridged_protocol;
}

namespace eka2l1::epoc::bt {
    class rfcomm_inet_protocol;

    enum rfcomm_option {
        rfcomm_opt_local_port_parameter = 0,
        rfcomm_opt_get_available_server_channel = 1,
        rfcomm_opt_maximum_supported_mtu = 2
    };

    class rfcomm_inet_socket : public btinet_socket {
    public:
        explicit rfcomm_inet_socket(rfcomm_inet_protocol *pr, std::unique_ptr<epoc::socket::socket> &inet_sock)
            : btinet_socket(reinterpret_cast<btlink_inet_protocol*>(pr), inet_sock) {
        }

        std::size_t get_option(const std::uint32_t option_id, const std::uint32_t option_family,
            std::uint8_t *buffer, const std::size_t avail_size) override;
    };

    class rfcomm_inet_protocol : public btlink_inet_protocol {
    private:
        epoc::internet::inet_bridged_protocol *inet_protocol_;

    public:
        explicit rfcomm_inet_protocol(midman *mid, epoc::internet::inet_bridged_protocol *inet_pro, const bool oldarch)
            : btlink_inet_protocol(mid, oldarch)
            , inet_protocol_(inet_pro) {
        }

        virtual std::u16string name() const override {
            return u"RFCOMM";
        }

        virtual std::vector<std::uint32_t> supported_ids() const override {
            return { RFCOMM_PROTOCOL_ID };
        }

        virtual std::int32_t message_size() const override;

        virtual std::unique_ptr<epoc::socket::socket> make_socket(const std::uint32_t family_id, const std::uint32_t protocol_id, const socket::socket_type sock_type) override;

        virtual std::unique_ptr<epoc::socket::socket> make_empty_base_link_socket() override {
            std::unique_ptr<epoc::socket::socket> empty_socket = std::unique_ptr<epoc::socket::socket>(nullptr);
            return std::make_unique<rfcomm_inet_socket>(this, empty_socket);
        }
    };
}
