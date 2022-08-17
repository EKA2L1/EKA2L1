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

// NOTE: This macro makes use of stack variable, so make sure this macro is called in the outer
// block or in the same block you want to use the address.
#define GUEST_TO_BSD_ADDR(addr, dest_ptr)                                                               \
    sockaddr_in ipv4_addr;                                                                              \
    sockaddr_in6 ipv6_addr;                                                                             \
    if (addr.family_ == epoc::internet::INET_ADDRESS_FAMILY) {                                          \
        ipv4_addr.sin_family = AF_INET;                                                                 \
        ipv4_addr.sin_port = htons(static_cast<std::uint16_t>(addr.port_));                             \
        std::memcpy(&ipv4_addr.sin_addr, addr.user_data_, 4);                                           \
        dest_ptr = reinterpret_cast<sockaddr*>(&ipv4_addr);                                             \
    } else if (addr.family_ == epoc::internet::INET6_ADDRESS_FAMILY) {                                                                                            \
        ipv6_addr.sin6_family = AF_INET6;                                                               \
        ipv6_addr.sin6_port = htons(static_cast<std::uint16_t>(addr.port_));                            \
        const epoc::internet::sinet6_address &ipv6_guest =                                              \
            static_cast<const epoc::internet::sinet6_address&>(addr);                                   \
        ipv6_addr.sin6_flowinfo = ipv6_guest.get_flow();                                                \
        ipv6_addr.sin6_scope_id = ipv6_guest.get_scope();                                               \
        std::memcpy(&ipv6_addr.sin6_addr, ipv6_guest.get_address_32x4(), 16);                           \
        dest_ptr = reinterpret_cast<sockaddr*>(&ipv6_addr);                                             \
    }

namespace eka2l1::epoc::internet {
    enum {
        INET_ADDRESS_FAMILY = 0x800,
        INET6_ADDRESS_FAMILY = 0x806,
        INET_ICMP_PROTCOL_ID = 1,
        INET_TCP_PROTOCOL_ID = 6,
        INET_UDP_PROTOCOL_ID = 17,

        // Option family. See in_sock.h
        INET_INTERFACE_CONTROL_OPT_FAMILY = 0x201,
        INET_ROUTE_CONTROL_OPT_FAMILY = 0x202,
        INET_DNS_CONTROL_OPT_FAMILY = 0x204,
        INET_TCP_SOCK_OPT_LEVEL = 0x106,
        
        // Option in IC family
        INET_ENUM_INTERFACES_OPT = 0x211,
        INET_NEXT_INTERFACE_OPT = 0x212,

        // Option in TCP socket option level
        INET_TCP_NO_DELAY_OPT = 0x304
    };
}