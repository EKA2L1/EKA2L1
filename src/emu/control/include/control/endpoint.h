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

#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace eka2l1::control {
    /**
     * \brief Where the control server listens.
     *
     * Written on the command line as either a local socket name - a socket file path on
     * Linux and macOS, a pipe name such as \\.\pipe\eka2l1 on Windows - or as
     * tcp:<host>:<port>, where the host must be a loopback address.
     */
    struct endpoint {
        enum kind_type {
            local,
            tcp
        };

        kind_type kind = local;

        // The socket path or pipe name for a local endpoint, the IP address for TCP.
        std::string address;
        std::uint16_t port = 0;

        /**
         * \brief Parse an endpoint as written on the command line.
         *
         * \param text  The endpoint text.
         * \param error Receives why the text was rejected.
         */
        static std::optional<endpoint> parse(const std::string &text, std::string &error);

        std::string to_string() const;
    };
}
