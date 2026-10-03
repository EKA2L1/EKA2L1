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

#include <chrono>
#include <climits>
#include <cstdlib>
#include <future>
#include <memory>
#include <thread>
#include <vector>

using namespace eka2l1::control;
using namespace eka2l1::control::test;

namespace {
    const std::string TOKEN = "0123456789abcdef";

    std::string request(const int id, const std::string &method, const std::string &params = "{}") {
        return R"({"jsonrpc":"2.0","id":)" + std::to_string(id) + R"(,"method":")" + method + R"(","params":)" + params + "}\n";
    }

    template <typename Predicate>
    bool wait_until(Predicate done) {
        for (int i = 0; (i < 500) && !done(); i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return done();
    }

    struct working_directory_guard {
        char saved[PATH_MAX];

        explicit working_directory_guard() {
            REQUIRE(getcwd(saved, sizeof(saved)) != nullptr);
        }

        ~working_directory_guard() {
            if (chdir(saved) != 0) {
                std::abort();
            }
        }
    };
}

TEST_CASE("a_local_socket_is_only_for_its_owner_whatever_the_umask", "[control][rpc_server]") {
    const std::string path = socket_path("mode");
    server_fixture fixture;

    const mode_t previous = umask(0);
    fixture.start(path);
    umask(previous);

    struct stat info;
    REQUIRE(stat(path.c_str(), &info) == 0);
    REQUIRE((info.st_mode & 0777) == 0600);
}

TEST_CASE("a_relative_socket_path_is_removed_where_it_was_made", "[control][rpc_server]") {
    const std::string path = socket_path("relative");
    working_directory_guard guard;
    const std::string absolute = std::string(guard.saved) + "/" + path;

    server_fixture fixture;
    fixture.start(path);

    // The emulator changes its working directory while it runs.
    REQUIRE(chdir("..") == 0);
    fixture.server.stop();

    struct stat info;
    REQUIRE(stat(absolute.c_str(), &info) != 0);
}

TEST_CASE("connections_past_the_limit_are_refused", "[control][rpc_server]") {
    const std::string path = socket_path("limit");
    server_fixture fixture;
    fixture.start(path);

    std::vector<std::unique_ptr<test_client>> clients;

    for (std::size_t i = 0; i < rpc_server::MAX_CONNECTIONS; i++) {
        clients.push_back(std::make_unique<test_client>());
        REQUIRE(clients.back()->connect_local(path));
        clients.back()->send_text(request(static_cast<int>(i), "echo"));
        REQUIRE(clients.back()->read_message()["id"].GetInt() == static_cast<int>(i));
    }

    test_client extra;
    REQUIRE(extra.connect_local(path));
    rapidjson::Document refusal = extra.read_message();
    REQUIRE(refusal["id"].IsNull());
    REQUIRE(refusal["error"]["code"].GetInt() == error_failed);
    REQUIRE(extra.read_line().empty());

    // A slot frees up once the server has seen a connection close.
    clients.pop_back();
    bool served = false;

    for (int attempt = 0; (attempt < 50) && !served; attempt++) {
        test_client late;
        REQUIRE(late.connect_local(path));
        late.send_text(request(99, "echo"));

        rapidjson::Document answer;
        answer.Parse(late.read_line().c_str());
        served = !answer.HasParseError() && answer.HasMember("result");

        if (!served) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    REQUIRE(served);
}

TEST_CASE("a_line_before_auth_is_limited_to_64_kib", "[control][rpc_server]") {
    server_fixture fixture;
    fixture.rpc.require_token(TOKEN);

    const std::uint16_t port = free_tcp_port();
    fixture.start("tcp:127.0.0.1:" + std::to_string(port));

    test_client stranger;
    REQUIRE(stranger.connect_tcp(port));
    stranger.send_text(std::string(64 * 1024 + 1, 'x'));

    rapidjson::Document refusal = stranger.read_message();
    REQUIRE(refusal["error"]["code"].GetInt() == error_parse);
    REQUIRE(stranger.read_line().empty());

    // Past auth, a line may be far longer.
    test_client client;
    REQUIRE(client.connect_tcp(port));
    client.send_text(request(1, "auth", R"({"token":")" + TOKEN + R"("})"));
    REQUIRE(client.read_message().HasMember("result"));

    const std::string long_text(1024 * 1024, 'y');
    client.send_text(request(2, "echo", R"({"text":")" + long_text + R"("})"));

    rapidjson::Document echo = client.read_message();
    REQUIRE(echo["id"].GetInt() == 2);
    REQUIRE(echo["result"]["text"].GetStringLength() == long_text.size());
}

TEST_CASE("a_line_is_limited_to_4_mib", "[control][rpc_server]") {
    const std::string path = socket_path("longline");
    server_fixture fixture;
    fixture.start(path);

    test_client client;
    REQUIRE(client.connect_local(path));
    client.send_text(std::string(4 * 1024 * 1024 + 1, 'x'));

    rapidjson::Document refusal = client.read_message();
    REQUIRE(refusal["error"]["code"].GetInt() == error_parse);
    REQUIRE(client.read_line().empty());
}

TEST_CASE("a_failed_auth_closes_the_connection", "[control][rpc_server]") {
    server_fixture fixture;
    fixture.rpc.require_token(TOKEN);

    const std::uint16_t port = free_tcp_port();
    fixture.start("tcp:127.0.0.1:" + std::to_string(port));

    test_client client;
    REQUIRE(client.connect_tcp(port));
    client.send_text(request(1, "auth", R"({"token":"0123456789abcdeX"})") + request(2, "auth", R"({"token":")" + TOKEN + R"("})"));

    rapidjson::Document refusal = client.read_message();
    REQUIRE(refusal["id"].GetInt() == 1);
    REQUIRE(refusal["error"]["code"].GetInt() == error_unauthorized);

    // The second guess in the same write is never looked at.
    REQUIRE(client.read_line().empty());
}

TEST_CASE("stop_does_not_wait_for_a_client_that_does_not_read", "[control][rpc_server]") {
    const std::string path = socket_path("deaf");
    server_fixture fixture;
    fixture.start(path);

    // 32 MiB of answers this client never reads.
    test_client client;
    REQUIRE(client.connect_local(path));
    for (int id = 1; id <= 8; id++) {
        client.send_text(request(id, "blob"));
    }

    REQUIRE(wait_until([&]() { return fixture.blob_calls > 0; }));

    std::promise<void> stopped;
    std::future<void> stopped_future = stopped.get_future();
    std::thread stopper([&]() {
        fixture.server.stop();
        stopped.set_value();
    });

    const bool in_time = stopped_future.wait_for(std::chrono::seconds(10)) == std::future_status::ready;

    if (!in_time) {
        // stop() is stuck for good; the process cannot be wound down cleanly.
        FAIL_CHECK("rpc_server::stop() did not return within 10 seconds");
        std::_Exit(1);
    }

    stopper.join();
}

TEST_CASE("a_client_that_does_not_read_is_not_served_until_it_does", "[control][rpc_server]") {
    const std::string path = socket_path("slow");
    server_fixture fixture;
    fixture.start(path);

    // 64 MiB of answers.
    test_client client;
    REQUIRE(client.connect_local(path));
    for (int id = 1; id <= 16; id++) {
        client.send_text(request(id, "blob"));
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    REQUIRE(fixture.blob_calls < 8);

    for (int id = 1; id <= 16; id++) {
        rapidjson::Document answer = client.read_message();
        REQUIRE(answer["id"].GetInt() == id);
    }

    REQUIRE(fixture.blob_calls == 16);
}

TEST_CASE("notifications_to_a_client_that_does_not_read_are_dropped", "[control][rpc_server]") {
    const std::string path = socket_path("dropped");
    server_fixture fixture;
    fixture.start(path);

    test_client slow;
    test_client fast;
    REQUIRE(slow.connect_local(path));
    REQUIRE(fast.connect_local(path));

    for (test_client *client : { &slow, &fast }) {
        client->send_text(request(1, "subscribe"));
        REQUIRE(client->read_message()["id"].GetInt() == 1);
    }

    // 12 MiB of answers the slow client does not read yet.
    slow.send_text(request(2, "blob") + request(3, "blob") + request(4, "blob"));

    REQUIRE(wait_until([&]() { return fixture.blob_calls == 3; }));

    // Both get the notification in the same server turn; the fast one shows it has happened.
    fixture.server.publish("ticks", R"({"jsonrpc":"2.0","method":"event.tick","params":{"n":1}})");
    REQUIRE(fast.read_message()["params"]["n"].GetInt() == 1);

    for (int id = 2; id <= 4; id++) {
        REQUIRE(slow.read_message()["id"].GetInt() == id);
    }

    fixture.server.publish("ticks", R"({"jsonrpc":"2.0","method":"event.tick","params":{"n":2}})");
    REQUIRE(fast.read_message()["params"]["n"].GetInt() == 2);
    REQUIRE(slow.read_message()["params"]["n"].GetInt() == 2);
}

#endif
