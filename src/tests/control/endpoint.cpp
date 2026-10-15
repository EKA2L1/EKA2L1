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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>
#include <control/endpoint.h>

using namespace eka2l1::control;

TEST_CASE("a_plain_name_is_a_local_socket", "[control][endpoint]") {
    std::string error;
    std::optional<endpoint> parsed = endpoint::parse("/run/user/1000/eka2l1.sock", error);

    REQUIRE(parsed.has_value());
    REQUIRE(parsed->kind == endpoint::local);
    REQUIRE(parsed->address == "/run/user/1000/eka2l1.sock");

    parsed = endpoint::parse(R"(\\.\pipe\eka2l1)", error);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->kind == endpoint::local);
    REQUIRE(parsed->address == R"(\\.\pipe\eka2l1)");
}

TEST_CASE("tcp_endpoints_take_a_loopback_host_and_a_port", "[control][endpoint]") {
    std::string error;
    std::optional<endpoint> parsed = endpoint::parse("tcp:127.0.0.1:5555", error);

    REQUIRE(parsed.has_value());
    REQUIRE(parsed->kind == endpoint::tcp);
    REQUIRE(parsed->address == "127.0.0.1");
    REQUIRE(parsed->port == 5555);
    REQUIRE(parsed->to_string() == "tcp:127.0.0.1:5555");

    parsed = endpoint::parse("tcp:localhost:1", error);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->address == "127.0.0.1");
    REQUIRE(parsed->port == 1);

    parsed = endpoint::parse("tcp:[::1]:65535", error);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->address == "::1");
    REQUIRE(parsed->port == 65535);
    REQUIRE(parsed->to_string() == "tcp:[::1]:65535");

    // Shorthand for the IPv4 loopback.
    parsed = endpoint::parse("tcp:5555", error);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->address == "127.0.0.1");
    REQUIRE(parsed->port == 5555);
}

TEST_CASE("bad_endpoints_are_rejected_with_a_reason", "[control][endpoint]") {
    for (const char *bad : { "", "tcp:", "tcp:127.0.0.1", "tcp:127.0.0.1:", "tcp:127.0.0.1:0",
             "tcp:127.0.0.1:65536", "tcp:127.0.0.1:12ab", "tcp:0.0.0.0:5555", "tcp:192.168.1.2:5555",
             "tcp:example.com:5555", "tcp:127.evil.com:5555", "tcp:[::]:5555", "tcp:[::1:5555" }) {
        INFO(bad);
        std::string error;
        REQUIRE_FALSE(endpoint::parse(bad, error).has_value());
        REQUIRE_FALSE(error.empty());
    }
}
