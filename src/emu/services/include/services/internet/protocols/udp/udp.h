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

#include <services/socket/protocol.h>
#include <services/internet/protocols/common.h>

#include <string>

namespace eka2l1::epoc::internet {
    class midman;
    class udp_protocol;

    class udp_host_resolver : public socket::host_resolver {
    private:
        udp_protocol *papa_;

    public:
        explicit udp_host_resolver(udp_protocol *papa);

        std::u16string host_name() const override;
        bool host_name(const std::u16string &name) override;
        
        bool get_by_address(epoc::socket::saddress &addr, epoc::socket::name_entry &result) override;
        bool get_by_name(epoc::socket::name_entry &supply_and_result) override;
    };

    class udp_protocol : public socket::protocol {
    public:
        explicit udp_protocol(const bool oldarch)
            : socket::protocol(oldarch) {
        }

        virtual std::u16string name() const override {
            return u"INetUDP";
        }

        std::uint32_t family_id() const override {
            return INET_ADDRESS_FAMILY;
        }

        virtual std::uint32_t id() const override {
            return INET_UDP_PROTOCOL_ID;
        }
        
        virtual epoc::version ver() const override {
            epoc::version v;
            v.major = 0;
            v.minor = 0;
            v.build = 1;

            return v;
        }

        virtual epoc::socket::byte_order get_byte_order() const override {
            return epoc::socket::byte_order_little_endian;
        }

        virtual std::unique_ptr<epoc::socket::socket> make_socket() override {
            // L2CAP will be the one handling this. This stack is currently only used for managing links.
            return nullptr;
        }
        
        virtual std::unique_ptr<epoc::socket::host_resolver> make_host_resolver() override {
            return std::make_unique<udp_host_resolver>(this);
        }
    };
}
