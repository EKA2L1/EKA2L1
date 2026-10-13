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

#include <memory>
#include <string>

namespace eka2l1::control {
    class dispatcher;
    class frontend;
    class server_impl;

    /**
     * \brief Lets other programs drive a running emulator: JSON-RPC 2.0 over a local socket.
     *
     * The protocol is described in src/emu/control/README.md. A frontend creates one server,
     * starts it with the endpoint the user asked for, and stops it before it destroys the
     * system, while its emulation and graphics threads still run.
     */
    class server {
        std::unique_ptr<server_impl> impl_;

    public:
        explicit server(frontend &host);
        ~server();

        server(const server &) = delete;
        server &operator=(const server &) = delete;

        /**
         * \brief Start listening.
         *
         * A TCP endpoint takes the token clients must present from the environment variable
         * EKA2L1_CONTROL_TOKEN, and does not start without one at least 16 bytes long.
         *
         * \param endpoint_text Where to listen, as written on the command line.
         * \param error         Receives why the server could not start.
         */
        bool start(const std::string &endpoint_text, std::string &error);

        /**
         * \brief Stop listening and close all connections. Safe to call more than once.
         */
        void stop();

        /**
         * \brief The method table, for frontends that add methods of their own.
         */
        dispatcher &get_dispatcher();
    };
}
