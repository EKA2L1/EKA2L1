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

#include <common/platform.h>

// The client side of these tests speaks POSIX sockets.
#if !EKA2L1_PLATFORM(WIN32)

#include "rpc_client.h"

using namespace eka2l1::control;
using namespace eka2l1::control::test;

TEST_CASE("requests_over_a_local_socket_are_answered_line_by_line", "[control][rpc_server]") {
    const std::string path = socket_path("lines");
    server_fixture fixture;
    fixture.start(path);

    test_client client;
    REQUIRE(client.connect_local(path));

    // Two requests in one write, the second one split over two writes.
    client.send_text("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"echo\",\"params\":{\"n\":1}}\n"
                     "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"echo\",");
    client.send_text("\"params\":{\"n\":2}}\r\n\n");

    rapidjson::Document first = client.read_message();
    REQUIRE(first["id"].GetInt() == 1);
    REQUIRE(first["result"]["n"].GetInt() == 1);

    rapidjson::Document second = client.read_message();
    REQUIRE(second["id"].GetInt() == 2);
    REQUIRE(second["result"]["n"].GetInt() == 2);

    // Garbage is answered, and the connection stays usable.
    client.send_text("not json\n");
    rapidjson::Document garbage = client.read_message();
    REQUIRE(garbage["error"]["code"].GetInt() == error_parse);

    client.send_text("{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"echo\"}\n");
    REQUIRE(client.read_message()["id"].GetInt() == 3);

    fixture.server.stop();

    // The server closed the connection and removed its socket file.
    REQUIRE(client.read_line().empty());
    struct stat info;
    REQUIRE(stat(path.c_str(), &info) != 0);
}

TEST_CASE("a_client_that_stops_sending_still_gets_every_answer", "[control][rpc_server]") {
    const std::string path = socket_path("halfclose");
    server_fixture fixture;
    fixture.start(path);

    test_client client;
    REQUIRE(client.connect_local(path));

    // As `nc -N` or a script piping its requests in does: send everything, then end the stream.
    client.send_text("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"blob\"}\n"
                     "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"echo\"}\n");
    REQUIRE(shutdown(client.fd, SHUT_WR) == 0);

    rapidjson::Document blob = client.read_message();
    REQUIRE(blob["id"].GetInt() == 1);
    REQUIRE(blob["result"]["blob"].GetStringLength() == 4 * 1024 * 1024);

    REQUIRE(client.read_message()["id"].GetInt() == 2);

    // Then the server closes its side.
    REQUIRE(client.read_line().empty());
}

TEST_CASE("published_lines_reach_only_subscribed_connections", "[control][rpc_server]") {
    const std::string path = socket_path("publish");
    server_fixture fixture;
    fixture.start(path);

    test_client subscriber;
    test_client bystander;
    REQUIRE(subscriber.connect_local(path));
    REQUIRE(bystander.connect_local(path));

    subscriber.send_text("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"subscribe\"}\n");
    REQUIRE(subscriber.read_message()["id"].GetInt() == 1);

    // Make sure the server has accepted the bystander before publishing.
    bystander.send_text("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"echo\"}\n");
    REQUIRE(bystander.read_message()["id"].GetInt() == 1);

    fixture.server.publish("ticks", R"({"jsonrpc":"2.0","method":"event.tick","params":{"n":1}})");
    fixture.server.publish("other", R"({"jsonrpc":"2.0","method":"event.other","params":{}})");

    rapidjson::Document tick = subscriber.read_message();
    REQUIRE(std::string(tick["method"].GetString()) == "event.tick");

    bystander.send_text("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"echo\"}\n");
    REQUIRE(bystander.read_message()["id"].GetInt() == 2);

    fixture.server.stop();
    REQUIRE(subscriber.read_line().empty());
    REQUIRE(bystander.read_line().empty());
}

TEST_CASE("a_live_socket_is_not_taken_over_but_a_stale_one_is", "[control][rpc_server]") {
    const std::string path = socket_path("takeover");

    server_fixture first;
    first.start(path);

    server_fixture second;
    std::string error;
    std::optional<endpoint> where = endpoint::parse(path, error);
    REQUIRE_FALSE(second.server.start(*where, error));
    REQUIRE(error.find("Another process") != std::string::npos);

    first.server.stop();

    // A socket file nobody listens on, as a crashed emulator leaves behind.
    const int stale = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);
    REQUIRE(bind(stale, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);
    close(stale);

    server_fixture third;
    third.start(path);

    test_client client;
    REQUIRE(client.connect_local(path));
    client.send_text("{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"echo\"}\n");
    REQUIRE(client.read_message()["id"].GetInt() == 9);
}

TEST_CASE("loopback_tcp_requires_the_token", "[control][rpc_server]") {
    server_fixture fixture;
    fixture.rpc.require_token("t0ken");

    const std::uint16_t port = free_tcp_port();

    fixture.start("tcp:127.0.0.1:" + std::to_string(port));

    test_client client;
    REQUIRE(client.connect_tcp(port));

    client.send_text("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"echo\"}\n");
    REQUIRE(client.read_message()["error"]["code"].GetInt() == error_unauthorized);

    client.send_text("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"auth\",\"params\":{\"token\":\"t0ken\"}}\n");
    REQUIRE(client.read_message().HasMember("result"));

    client.send_text("{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"echo\"}\n");
    REQUIRE(client.read_message().HasMember("result"));
}

#endif
