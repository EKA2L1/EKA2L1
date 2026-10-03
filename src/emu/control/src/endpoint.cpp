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

#include <control/endpoint.h>

#include <algorithm>
#include <cctype>

namespace eka2l1::control {
    static constexpr const char *TCP_PREFIX = "tcp:";

    static std::optional<std::uint16_t> parse_port(const std::string &text) {
        if (text.empty() || (text.size() > 5) || !std::all_of(text.begin(), text.end(), [](const char c) { return std::isdigit(static_cast<unsigned char>(c)); })) {
            return std::nullopt;
        }

        const unsigned long port = std::stoul(text);

        if ((port == 0) || (port > 65535)) {
            return std::nullopt;
        }

        return static_cast<std::uint16_t>(port);
    }

    std::optional<endpoint> endpoint::parse(const std::string &text, std::string &error) {
        if (text.empty()) {
            error = "The control endpoint is empty";
            return std::nullopt;
        }

        endpoint result;

        if (text.rfind(TCP_PREFIX, 0) != 0) {
            result.kind = local;
            result.address = text;

            return result;
        }

        result.kind = tcp;

        std::string rest = text.substr(std::char_traits<char>::length(TCP_PREFIX));
        std::string host = "127.0.0.1";
        std::string port_text = rest;

        if (!rest.empty() && (rest[0] == '[')) {
            const std::size_t close = rest.find(']');

            if ((close == std::string::npos) || (close + 1 >= rest.size()) || (rest[close + 1] != ':')) {
                error = "Write an IPv6 TCP endpoint as tcp:[::1]:<port>";
                return std::nullopt;
            }

            host = rest.substr(1, close - 1);
            port_text = rest.substr(close + 2);
        } else if (const std::size_t colon = rest.rfind(':'); colon != std::string::npos) {
            host = rest.substr(0, colon);
            port_text = rest.substr(colon + 1);
        }

        if (host == "localhost") {
            host = "127.0.0.1";
        }

        // Anything that can reach the control server can install and run code in the guest,
        // so it never listens beyond this machine.
        const bool ipv4_loopback = (host.rfind("127.", 0) == 0)
            && std::all_of(host.begin(), host.end(), [](const char c) { return std::isdigit(static_cast<unsigned char>(c)) || (c == '.'); });

        if ((host != "::1") && !ipv4_loopback) {
            error = "The control server only listens on a loopback address (127.0.0.1, ::1 or localhost), not on '" + host + "'";
            return std::nullopt;
        }

        const std::optional<std::uint16_t> port = parse_port(port_text);

        if (!port) {
            error = "The TCP port must be a number from 1 to 65535: tcp:127.0.0.1:<port>";
            return std::nullopt;
        }

        result.address = host;
        result.port = *port;

        return result;
    }

    std::string endpoint::to_string() const {
        if (kind == local) {
            return address;
        }

        const bool ipv6 = (address.find(':') != std::string::npos);
        return std::string(TCP_PREFIX) + (ipv6 ? "[" + address + "]" : address) + ":" + std::to_string(port);
    }
}
