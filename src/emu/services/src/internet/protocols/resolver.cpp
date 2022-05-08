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
#include <common/platform.h>
#include <services/internet/protocols/inet.h>

#if EKA2L1_PLATFORM(WIN32)
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#endif

namespace eka2l1::epoc::internet {
    inet_host_resolver::inet_host_resolver(inet_bridged_protocol *papa, const std::uint32_t address_family, const std::uint32_t protocol_id)
        : papa_(papa)
        , addr_family_(address_family)
        , protocol_id_(protocol_id)
        , prev_info_(nullptr)
        , iterating_info_(nullptr) {
    }

    inet_host_resolver::~inet_host_resolver() {
        if (prev_info_) {
            freeaddrinfo(prev_info_);
        }
    }

    std::u16string inet_host_resolver::host_name() const {
        // I don't think this has much meaning
        return u"";
    }

    bool inet_host_resolver::host_name(const std::u16string &name) {
        return true;
    }

    static void addrinfo_to_name_entry(epoc::socket::name_entry &supply_and_result, addrinfo *result_info) {
        if (result_info->ai_family == AF_INET6) {
            supply_and_result.addr_.family_ = INET6_ADDRESS_FAMILY;

            sockaddr_in6 *in6 = reinterpret_cast<sockaddr_in6*>(result_info->ai_addr);
            supply_and_result.addr_.port_ = ntohs(in6->sin6_port);

            sinet6_address &in6_guest = static_cast<sinet6_address&>(supply_and_result.addr_);
            std::memcpy(in6_guest.address_32x4(), &in6->sin6_addr, 16);

            *in6_guest.flow() = in6->sin6_flowinfo;
            *in6_guest.scope() = in6->sin6_scope_id;
        } else {
            supply_and_result.addr_.family_ = INET_ADDRESS_FAMILY;

            sinet_address &in4_guest = static_cast<sinet_address&>(supply_and_result.addr_);
            sockaddr_in *in = reinterpret_cast<sockaddr_in*>(result_info->ai_addr);

            std::memcpy(in4_guest.addr_long(), &in->sin_addr, 4);
            in4_guest.port_ = ntohs(in->sin_port);
        }
    }

    bool inet_host_resolver::next(epoc::socket::name_entry &result) {
        if (!iterating_info_) {
            return false;
        }

        addrinfo_to_name_entry(result, iterating_info_);
        iterating_info_ = iterating_info_->ai_next;

        return true;
    }

    bool inet_host_resolver::get_by_address(epoc::socket::saddress &addr, epoc::socket::name_entry &result) {
        LOG_WARN(SERVICE_INTERNET, "Get host by address stubbed to not found");
        return false;
    }

    bool inet_host_resolver::get_by_name(epoc::socket::name_entry &supply_and_result) {
        const std::string name_utf8 = common::ucs2_to_utf8(supply_and_result.name_.to_std_string(nullptr));
        
        if (prev_info_) {
            freeaddrinfo(prev_info_);
        }

        // Set hint
        addrinfo hint_info;
        std::memset(&hint_info, 0, sizeof(addrinfo));

        hint_info.ai_family = (addr_family_ == INET6_ADDRESS_FAMILY) ? AF_INET6 : AF_INET;
        hint_info.ai_socktype = (protocol_id_ == INET_UDP_PROTOCOL_ID) ? SOCK_DGRAM : SOCK_STREAM; 
        hint_info.ai_protocol = (protocol_id_ == INET_UDP_PROTOCOL_ID) ? IPPROTO_UDP : IPPROTO_TCP;

        addrinfo *result_info = nullptr;
        const int result_code = getaddrinfo(name_utf8.c_str(), nullptr, &hint_info, &result_info);

        if (result_code != 0) {
            LOG_ERROR(SERVICE_INTERNET, "Get address by name failed with code {}", result_code);
            return false;
        }

        if (!result_info || !result_info->ai_addr) {
            LOG_ERROR(SERVICE_INTERNET, "Address retrieve is not fullfilled!");
            return false;
        }

        addrinfo_to_name_entry(supply_and_result, result_info);

        prev_info_ = result_info;
        iterating_info_ = result_info->ai_next;

        return true;
    }
}