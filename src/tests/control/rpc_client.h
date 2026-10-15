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

// Client side of the transport tests: POSIX sockets only.

#pragma once

#include <catch2/catch.hpp>

#include <control/dispatcher.h>
#include <control/endpoint.h>
#include <control/rpc_server.h>

#include <rapidjson/document.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

namespace eka2l1::control::test {
    inline std::string socket_path(const char *name) {
        return "ekatests-" + std::to_string(getpid()) + "-" + name + ".sock";
    }

    struct test_client {
        int fd = -1;
        std::string buffer;

        ~test_client() {
            if (fd >= 0) {
                close(fd);
            }
        }

        bool connect_local(const std::string &path) {
            fd = socket(AF_UNIX, SOCK_STREAM, 0);

            sockaddr_un address{};
            address.sun_family = AF_UNIX;
            std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);

            return connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0;
        }

        bool connect_tcp(const std::uint16_t port) {
            fd = socket(AF_INET, SOCK_STREAM, 0);

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(port);
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

            return connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0;
        }

        // MSG_NOSIGNAL: a server that closes the connection must fail the test, not kill it.
        void send_text(const std::string &text) {
            std::size_t sent = 0;

            while (sent < text.size()) {
                const ssize_t written = send(fd, text.data() + sent, text.size() - sent, MSG_NOSIGNAL);
                REQUIRE(written > 0);
                sent += static_cast<std::size_t>(written);
            }
        }

        // Next line from the server, or empty on EOF or after five seconds of silence.
        std::string read_line() {
            std::size_t scanned = 0;

            while (true) {
                const std::size_t newline = buffer.find('\n', scanned);

                if (newline != std::string::npos) {
                    std::string line = buffer.substr(0, newline);
                    buffer.erase(0, newline + 1);
                    return line;
                }

                scanned = buffer.size();

                pollfd waiting{ fd, POLLIN, 0 };
                if (poll(&waiting, 1, 5000) <= 0) {
                    return std::string();
                }

                char chunk[65536];
                const ssize_t got = read(fd, chunk, sizeof(chunk));

                if (got <= 0) {
                    return std::string();
                }

                buffer.append(chunk, static_cast<std::size_t>(got));
            }
        }

        rapidjson::Document read_message() {
            const std::string line = read_line();
            REQUIRE_FALSE(line.empty());

            rapidjson::Document doc;
            doc.Parse(line.c_str());
            REQUIRE_FALSE(doc.HasParseError());

            return doc;
        }
    };

    struct server_fixture {
        dispatcher rpc;
        rpc_server server;
        std::atomic<int> blob_calls{ 0 };

        explicit server_fixture()
            : server(rpc) {
            rpc.add("echo", [](session &, const rapidjson::Value &params, rapidjson::Value &result, json_allocator &allocator) {
                result.CopyFrom(params, allocator);
            });

            rpc.add("subscribe", [](session &client, const rapidjson::Value &, rapidjson::Value &, json_allocator &) {
                client.subscriptions.insert("ticks");
            });

            // An answer far larger than a socket buffer, so writing it takes many loop turns.
            rpc.add("blob", [this](session &, const rapidjson::Value &, rapidjson::Value &result, json_allocator &allocator) {
                blob_calls++;
                const std::string blob(4 * 1024 * 1024, 'x');
                result.AddMember("blob", rapidjson::Value(blob.c_str(), static_cast<rapidjson::SizeType>(blob.size()), allocator), allocator);
            });
        }

        void start(const std::string &text) {
            std::string error;
            const std::optional<endpoint> where = endpoint::parse(text, error);
            REQUIRE(where.has_value());

            INFO(error);
            REQUIRE(server.start(*where, error));
        }
    };

    // A loopback TCP port nobody listens on right now.
    inline std::uint16_t free_tcp_port() {
        const int probe = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        REQUIRE(bind(probe, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);
        socklen_t length = sizeof(address);
        getsockname(probe, reinterpret_cast<sockaddr *>(&address), &length);
        close(probe);

        return ntohs(address.sin_port);
    }
}
