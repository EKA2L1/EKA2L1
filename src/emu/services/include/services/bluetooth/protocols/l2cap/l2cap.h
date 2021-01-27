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

#include <services/bluetooth/protocols/btlink/btlink.h>
#include <services/socket/socket.h>

namespace eka2l1::epoc::bt {
    class l2cap_protocol;

    enum l2cap_socket_option_oldarch {
        l2cap_socket_link_count = 0,
        l2cap_socket_link_array = 1
    };
    
    class l2cap_socket: public socket::socket {
    private:
        l2cap_protocol *pr_;

    public:
        explicit l2cap_socket(l2cap_protocol *pr)
            : pr_(pr) {
        }
        
        virtual std::size_t get_option(const std::uint32_t option_id, const std::uint32_t option_family,
            std::uint8_t *buffer, const std::size_t avail_size) override;
        
        virtual bool set_option(const std::uint32_t option_id, const std::uint32_t option_family,
            std::uint8_t *buffer, const std::size_t avail_size) override;
    };

    class l2cap_protocol : public btlink_protocol {
    public:
        explicit l2cap_protocol(midman *mid, const bool oldarch)
            : btlink_protocol(mid, oldarch) {
        }

        virtual std::u16string name() const override {
            return u"L2CAP";
        }

        virtual std::uint32_t id() const override {
            return L2CAP_PROTOCOL_ID;
        }
        
        virtual std::unique_ptr<epoc::socket::socket> make_socket() override {
            return std::make_unique<l2cap_socket>(this);
        }
    };
}
