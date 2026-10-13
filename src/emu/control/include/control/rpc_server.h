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

#include <control/endpoint.h>

#include <cstddef>
#include <memory>
#include <string>

namespace eka2l1::control {
    class dispatcher;
    class rpc_server_impl;

    /**
     * \brief Serves a dispatcher over a local socket or loopback TCP, one JSON message per line.
     *
     * The server runs its own libuv loop on a thread of its own. Messages are handled one at a
     * time, in the order they arrive, on that thread, so a method handler may block it.
     *
     * A line may be 4 MiB long, or 64 KiB while the connection has not passed auth. A failed
     * auth closes the connection. A connection whose unsent answers pass 8 MiB is not read or
     * served, and misses its notifications, until its client has read them down to 4 MiB.
     */
    class rpc_server {
        std::unique_ptr<rpc_server_impl> impl_;

    public:
        /**
         * \brief Connections served at once. One more is answered with an error and closed.
         */
        static constexpr std::size_t MAX_CONNECTIONS = 8;

        explicit rpc_server(dispatcher &rpc);
        ~rpc_server();

        rpc_server(const rpc_server &) = delete;
        rpc_server &operator=(const rpc_server &) = delete;

        /**
         * \brief Start listening.
         *
         * A local socket file left behind by a process that is gone is replaced; one that
         * another process still listens on is not. A new socket file is made readable and
         * writable by its owner only, and a relative path is taken from the current directory.
         *
         * \param where Where to listen.
         * \param error Receives why the server could not start.
         */
        bool start(const endpoint &where, std::string &error);

        /**
         * \brief Stop listening and close every connection, after writing what was already queued.
         *
         * A connection whose client does not read what is queued is closed after two seconds.
         * Waits for the server thread to finish. Safe to call more than once.
         */
        void stop();

        /**
         * \brief Send a notification line to every connection subscribed to a topic. Thread-safe.
         */
        void publish(const std::string &topic, const std::string &line);
    };
}
