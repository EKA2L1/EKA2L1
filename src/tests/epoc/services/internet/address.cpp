/*
 * Copyright (c) 2026 EKA2L1 Team.
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>

#include <common/platform.h>
#include <services/internet/protocols/common.h>
#include <services/internet/protocols/inet.h>

#if EKA2L1_PLATFORM(WIN32)
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>
#endif

#include <cstring>

using namespace eka2l1;

namespace {
    // in_sock.h:
    //   #define INET_ADDR(a,b,c,d) (TUint32)((((TUint32)(a))<<24)|((b)<<16)|((c)<<8)|(d))
    //   const TUint32 KInetAddrLoop = INET_ADDR(127,0,0,1);
    // That word is what TInetAddr::Address() hands back and what TInetAddr::Output()
    // formats, so it is the layout a guest reads an address in.
    constexpr std::uint32_t inet_addr_of(const std::uint8_t a, const std::uint8_t b, const std::uint8_t c,
        const std::uint8_t d) {
        return (static_cast<std::uint32_t>(a) << 24) | (b << 16) | (c << 8) | d;
    }

    constexpr std::uint32_t K_INET_ADDR_LOOP = inet_addr_of(127, 0, 0, 1);

    sockaddr_in make_host_v4(const std::uint32_t addr, const std::uint16_t port) {
        sockaddr_in host_addr;
        std::memset(&host_addr, 0, sizeof(host_addr));

        host_addr.sin_family = AF_INET;
        host_addr.sin_port = htons(port);
        host_addr.sin_addr.s_addr = htonl(addr);

        return host_addr;
    }
}

TEST_CASE("An IPv4 address reaches the guest as TInetAddr::Address() reads it", "[internet]") {
    sockaddr_in host_addr = make_host_v4(K_INET_ADDR_LOOP, 8080);

    epoc::socket::saddress guest_addr;
    std::memset(&guest_addr, 0, sizeof(guest_addr));

    epoc::internet::host_sockaddr_to_guest_saddress(reinterpret_cast<sockaddr *>(&host_addr), guest_addr);

    REQUIRE(guest_addr.family_ == epoc::internet::INET_ADDRESS_FAMILY);
    REQUIRE(guest_addr.port_ == 8080);
    REQUIRE(*static_cast<epoc::internet::sinet_address &>(guest_addr).addr_long() == K_INET_ADDR_LOOP);

    host_addr = make_host_v4(inet_addr_of(198, 18, 15, 95), 80);
    epoc::internet::host_sockaddr_to_guest_saddress(reinterpret_cast<sockaddr *>(&host_addr), guest_addr);

    REQUIRE(*static_cast<epoc::internet::sinet_address &>(guest_addr).addr_long() == inet_addr_of(198, 18, 15, 95));
}

TEST_CASE("An address a guest built with INET_ADDR goes out to the host correctly", "[internet]") {
    epoc::socket::saddress guest_addr;
    std::memset(&guest_addr, 0, sizeof(guest_addr));

    guest_addr.family_ = epoc::internet::INET_ADDRESS_FAMILY;
    guest_addr.port_ = 8080;
    *static_cast<epoc::internet::sinet_address &>(guest_addr).addr_long() = K_INET_ADDR_LOOP;

    sockaddr *converted = nullptr;
    GUEST_TO_BSD_ADDR(guest_addr, converted);

    REQUIRE(converted != nullptr);

    const sockaddr_in *converted_v4 = reinterpret_cast<const sockaddr_in *>(converted);
    REQUIRE(converted_v4->sin_family == AF_INET);
    REQUIRE(ntohs(converted_v4->sin_port) == 8080);
    REQUIRE(converted_v4->sin_addr.s_addr == htonl(K_INET_ADDR_LOOP));
}
