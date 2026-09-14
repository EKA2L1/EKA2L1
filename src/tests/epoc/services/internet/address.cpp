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
#include <config/config.h>
#include <services/internet/protocols/common.h>
#include <services/internet/protocols/inet.h>
#include <utils/err.h>

#if EKA2L1_PLATFORM(WIN32)
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include <netinet/in.h>
#endif

#include <cstring>

using namespace eka2l1;

TEST_CASE("Internet protocol descriptions distinguish TCP and UDP", "[internet]") {
    // in_sock.h and es_sock.h define the IDs and socket types returned by FindProtocol.
    epoc::internet::inet_bridged_protocol tcp(nullptr, true, 6);
    epoc::internet::inet_bridged_protocol udp(nullptr, true, 17);

    REQUIRE(tcp.name() == u"tcp");
    REQUIRE(tcp.supported_ids() == std::vector<std::uint32_t>{6});
    REQUIRE(tcp.sock_type() == 1);
    REQUIRE(tcp.message_size() == 0);
    REQUIRE(tcp.family_ids().front() == 0x800);
    REQUIRE(udp.name() == u"udp");
    REQUIRE(udp.supported_ids() == std::vector<std::uint32_t>{17});
    REQUIRE(udp.sock_type() == 2);
    REQUIRE(udp.family_ids().front() == 0x800);
}

TEST_CASE("Host overrides match complete DNS names", "[internet][config]") {
    config::state settings;
    settings.hosts = {{"Game.Example.", "127.0.0.1"}, {"ipv6.example", "::1"}};

    REQUIRE(settings.host_override("GAME.example") == "127.0.0.1");
    REQUIRE(settings.host_override("game.example.") == "127.0.0.1");
    REQUIRE(settings.host_override("IPV6.EXAMPLE.") == "::1");
    REQUIRE_FALSE(settings.host_override("sub.game.example"));
    REQUIRE_FALSE(settings.host_override("game.example.invalid"));
    REQUIRE_FALSE(settings.host_override("other.example"));
    settings.hosts.clear();
    REQUIRE_FALSE(settings.host_override("game.example"));
}

TEST_CASE("Host mappings accept DNS targets without recursive rewriting", "[internet][config]") {
    config::state settings;
    settings.hosts = {{"Arena.Example.", " Private.Example. "}, {"private.example", "127.0.0.1"}};
    REQUIRE(settings.host_override("ARENA.EXAMPLE") == "private.example");
    REQUIRE(settings.host_override("private.example") == "127.0.0.1");
    REQUIRE(config::normalize_host_name(" \tGame.Example.\r\n") == "game.example");
    REQUIRE(config::normalize_host_name(" \t\r\n").empty());
}

TEST_CASE("Host mapping validation accepts addresses and bounds DNS labels", "[internet][config]") {
    for (const auto *target : {"127.0.0.1", "2001:db8::1", "::1", "private.example", "localhost", "xn--bcher-kva.example"}) {
        INFO(target);
        CHECK(config::valid_host_target(target));
    }
    for (const auto *target : {"", "https://private.example", "private.example:8192", "[::1]", "1.2.3.999",
            "private..example", "private/example", "private example", "::invalid"}) {
        INFO(target);
        CHECK_FALSE(config::valid_host_target(target));
    }
    CHECK(config::valid_host_name(std::string(63, 'a') + ".example"));
    CHECK_FALSE(config::valid_host_name(std::string(64, 'a') + ".example"));
    CHECK_FALSE(config::valid_host_name(std::string(254, 'a')));
    CHECK_FALSE(config::valid_host_target(std::string("127.0.0.1\0.example", 18)));
    CHECK(config::numeric_host_address("::ffff:192.0.2.1"));
    CHECK_FALSE(config::numeric_host_address("localhost"));
}

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

TEST_CASE("DNS results contain a complete copyable TSockAddr", "[internet]") {
    sockaddr_in address = make_host_v4(K_INET_ADDR_LOOP, 0);
    addrinfo resolved{};
    resolved.ai_family = AF_INET;
    resolved.ai_addr = reinterpret_cast<sockaddr *>(&address);
    epoc::socket::name_entry entry;
    entry.length_ = 0x30000008;
    entry.flags_ = epoc::socket::name_entry::FLAG_ALIAS_NAME;

    epoc::internet::addrinfo_to_name_entry(entry, &resolved);

    // TSockAddr starts with just its family and port; copying the descriptor must include the IPv4 word.
    REQUIRE(entry.length_ == 0x3000000C);
    REQUIRE(entry.max_length_ == 32);
    REQUIRE(entry.flags_ == 0);
    REQUIRE(*static_cast<epoc::internet::sinet_address &>(entry.addr_).addr_long() == K_INET_ADDR_LOOP);

    sockaddr_in6 address6{};
    address6.sin6_family = AF_INET6;
    address6.sin6_addr.s6_addr[15] = 1;
    resolved.ai_family = AF_INET6;
    resolved.ai_addr = reinterpret_cast<sockaddr *>(&address6);
    epoc::internet::addrinfo_to_name_entry(entry, &resolved);
    REQUIRE(entry.length_ == 0x30000020);
    REQUIRE(entry.addr_.family_ == epoc::internet::INET6_ADDRESS_FAMILY);
}

TEST_CASE("SetLocalPort binds an unspecified TSockAddr to the socket family", "[internet]") {
    epoc::socket::saddress guest_addr;
    std::memset(&guest_addr, 0xA5, sizeof(guest_addr));
    guest_addr.family_ = 0;
    guest_addr.port_ = 2000;
    sockaddr_in6 host_addr;

    REQUIRE(epoc::internet::guest_bind_address_to_host(guest_addr, 0x800, host_addr) == epoc::error_none);
    const auto &ipv4 = reinterpret_cast<const sockaddr_in &>(host_addr);
    REQUIRE(ipv4.sin_family == AF_INET);
    REQUIRE(ntohs(ipv4.sin_port) == 2000);
    REQUIRE(ipv4.sin_addr.s_addr == htonl(INADDR_ANY));

    REQUIRE(epoc::internet::guest_bind_address_to_host(guest_addr, 0x806, host_addr) == epoc::error_none);
    REQUIRE(host_addr.sin6_family == AF_INET6);
    REQUIRE(ntohs(host_addr.sin6_port) == 2000);
    REQUIRE(IN6_IS_ADDR_UNSPECIFIED(&host_addr.sin6_addr));
    REQUIRE(host_addr.sin6_flowinfo == 0);
    REQUIRE(host_addr.sin6_scope_id == 0);

    guest_addr.port_ = 0;
    REQUIRE(epoc::internet::guest_bind_address_to_host(guest_addr, 0x800, host_addr) == epoc::error_none);
    REQUIRE(reinterpret_cast<const sockaddr_in &>(host_addr).sin_port == 0);
}

TEST_CASE("Bind retains explicit addresses and rejects invalid arguments", "[internet]") {
    epoc::socket::saddress guest_addr{};
    guest_addr.family_ = 0x800;
    guest_addr.port_ = 65535;
    *static_cast<epoc::internet::sinet_address &>(guest_addr).addr_long() = K_INET_ADDR_LOOP;
    sockaddr_in6 host_addr;

    REQUIRE(epoc::internet::guest_bind_address_to_host(guest_addr, 0x800, host_addr) == epoc::error_none);
    REQUIRE(reinterpret_cast<const sockaddr_in &>(host_addr).sin_addr.s_addr == htonl(K_INET_ADDR_LOOP));
    REQUIRE(ntohs(reinterpret_cast<const sockaddr_in &>(host_addr).sin_port) == 65535);

    guest_addr.port_ = 65536;
    REQUIRE(epoc::internet::guest_bind_address_to_host(guest_addr, 0x800, host_addr) == epoc::error_too_big);
    guest_addr.port_ = 2000;
    guest_addr.family_ = epoc::socket::INVALID_FAMILY_ID;
    REQUIRE(epoc::internet::guest_bind_address_to_host(guest_addr, 0x800, host_addr) == epoc::error_argument);
}
