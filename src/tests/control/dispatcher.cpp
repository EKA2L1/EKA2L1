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
#include <control/dispatcher.h>

#include <rapidjson/document.h>

#include <stdexcept>
#include <string>
#include <thread>

using namespace eka2l1::control;

namespace {
    struct dispatcher_fixture {
        dispatcher rpc;
        session client;
        int echo_calls = 0;

        explicit dispatcher_fixture() {
            rpc.add("echo", [this](session &, const rapidjson::Value &params, rapidjson::Value &result, json_allocator &allocator) {
                echo_calls++;
                result.CopyFrom(params, allocator);
            });

            rpc.add("fail", [](session &, const rapidjson::Value &, rapidjson::Value &, json_allocator &) {
                throw rpc_error(error_not_found, "No such app");
            });

            rpc.add("crash", [](session &, const rapidjson::Value &, rapidjson::Value &, json_allocator &) {
                throw std::runtime_error("boom");
            });
        }

        rapidjson::Document call(const std::string &line) {
            const std::string response = rpc.handle(client, line);

            rapidjson::Document doc;
            doc.Parse(response.c_str());
            REQUIRE_FALSE(doc.HasParseError());

            return doc;
        }
    };

    void require_error(const rapidjson::Value &response, const int code) {
        REQUIRE(response.IsObject());
        REQUIRE(std::string(response["jsonrpc"].GetString()) == "2.0");
        REQUIRE(response.HasMember("error"));
        REQUIRE_FALSE(response.HasMember("result"));
        REQUIRE(response["error"]["code"].GetInt() == code);
        REQUIRE(response["error"]["message"].IsString());
    }
}

TEST_CASE("a_request_gets_its_result_under_the_same_id", "[control][dispatcher]") {
    dispatcher_fixture fixture;

    rapidjson::Document response = fixture.call(R"({"jsonrpc":"2.0","id":7,"method":"echo","params":{"x":1}})");
    REQUIRE(std::string(response["jsonrpc"].GetString()) == "2.0");
    REQUIRE(response["id"].GetInt() == 7);
    REQUIRE(response["result"]["x"].GetInt() == 1);
    REQUIRE_FALSE(response.HasMember("error"));

    response = fixture.call(R"({"jsonrpc":"2.0","id":"abc","method":"echo"})");
    REQUIRE(std::string(response["id"].GetString()) == "abc");
    REQUIRE(response["result"].IsObject());
    REQUIRE(response["result"].MemberCount() == 0);
}

TEST_CASE("a_notification_runs_but_is_not_answered", "[control][dispatcher]") {
    dispatcher_fixture fixture;

    REQUIRE(fixture.rpc.handle(fixture.client, R"({"jsonrpc":"2.0","method":"echo"})").empty());
    REQUIRE(fixture.echo_calls == 1);

    // Errors in notifications are not reported either.
    REQUIRE(fixture.rpc.handle(fixture.client, R"({"jsonrpc":"2.0","method":"nope"})").empty());
}

TEST_CASE("malformed_messages_get_standard_errors", "[control][dispatcher]") {
    dispatcher_fixture fixture;

    rapidjson::Document response = fixture.call(R"({"jsonrpc":"2.0","id":1,"method":)");
    require_error(response, error_parse);
    REQUIRE(response["id"].IsNull());

    response = fixture.call("42");
    require_error(response, error_invalid_request);
    REQUIRE(response["id"].IsNull());

    response = fixture.call(R"({"jsonrpc":"1.0","id":3,"method":"echo"})");
    require_error(response, error_invalid_request);
    REQUIRE(response["id"].GetInt() == 3);

    response = fixture.call(R"({"id":4,"method":"echo"})");
    require_error(response, error_invalid_request);

    response = fixture.call(R"({"jsonrpc":"2.0","id":5,"method":12})");
    require_error(response, error_invalid_request);

    response = fixture.call(R"({"jsonrpc":"2.0","id":{"a":1},"method":"echo"})");
    require_error(response, error_invalid_request);
    REQUIRE(response["id"].IsNull());

    response = fixture.call(R"({"jsonrpc":"2.0","id":6,"method":"echo","params":[1,2]})");
    require_error(response, error_invalid_params);
    REQUIRE(response["id"].GetInt() == 6);

    REQUIRE(fixture.echo_calls == 0);
}

TEST_CASE("an_unknown_method_is_reported", "[control][dispatcher]") {
    dispatcher_fixture fixture;

    rapidjson::Document response = fixture.call(R"({"jsonrpc":"2.0","id":1,"method":"nope"})");
    require_error(response, error_method_not_found);
    REQUIRE(response["id"].GetInt() == 1);
}

TEST_CASE("handler_failures_become_errors", "[control][dispatcher]") {
    dispatcher_fixture fixture;

    rapidjson::Document response = fixture.call(R"({"jsonrpc":"2.0","id":1,"method":"fail"})");
    require_error(response, error_not_found);
    REQUIRE(std::string(response["error"]["message"].GetString()) == "No such app");

    response = fixture.call(R"({"jsonrpc":"2.0","id":2,"method":"crash"})");
    require_error(response, error_internal);
    REQUIRE(std::string(response["error"]["message"].GetString()) == "boom");
}

TEST_CASE("a_batch_is_answered_with_an_array", "[control][dispatcher]") {
    dispatcher_fixture fixture;

    rapidjson::Document response = fixture.call(R"([
        {"jsonrpc":"2.0","id":1,"method":"echo","params":{"n":1}},
        {"jsonrpc":"2.0","method":"echo"},
        {"jsonrpc":"2.0","id":2,"method":"nope"},
        5
    ])");

    REQUIRE(response.IsArray());
    REQUIRE(response.Size() == 3);
    REQUIRE(response[0]["result"]["n"].GetInt() == 1);
    require_error(response[1], error_method_not_found);
    require_error(response[2], error_invalid_request);
    REQUIRE(fixture.echo_calls == 2);

    require_error(fixture.call("[]"), error_invalid_request);
    REQUIRE(fixture.rpc.handle(fixture.client, R"([{"jsonrpc":"2.0","method":"echo"}])").empty());
}

TEST_CASE("a_token_must_be_presented_before_anything_else", "[control][dispatcher]") {
    dispatcher_fixture fixture;
    fixture.rpc.require_token("s3cret");

    rapidjson::Document response = fixture.call(R"({"jsonrpc":"2.0","id":1,"method":"echo"})");
    require_error(response, error_unauthorized);

    // Method names are not given away before auth.
    response = fixture.call(R"({"jsonrpc":"2.0","id":2,"method":"nope"})");
    require_error(response, error_unauthorized);

    response = fixture.call(R"({"jsonrpc":"2.0","id":3,"method":"auth","params":{}})");
    require_error(response, error_invalid_params);
    REQUIRE_FALSE(fixture.client.rejected);

    response = fixture.call(R"({"jsonrpc":"2.0","id":4,"method":"auth","params":{"token":"s3cret"}})");
    REQUIRE(response["result"].IsObject());
    REQUIRE(fixture.client.authenticated);

    response = fixture.call(R"({"jsonrpc":"2.0","id":5,"method":"echo"})");
    REQUIRE(response.HasMember("result"));
    REQUIRE(fixture.echo_calls == 1);
}

TEST_CASE("auth_is_accepted_when_no_token_is_required", "[control][dispatcher]") {
    dispatcher_fixture fixture;

    rapidjson::Document response = fixture.call(R"({"jsonrpc":"2.0","id":1,"method":"auth","params":{"token":"anything"}})");
    REQUIRE(response.HasMember("result"));
}

TEST_CASE("notifications_are_one_line_requests_without_id", "[control][dispatcher]") {
    rapidjson::Document params;
    params.SetObject();
    params.AddMember("uid", 0xE7351C2F, params.GetAllocator());

    const std::string line = make_notification("event.app_exited", params);
    REQUIRE(line.find('\n') == std::string::npos);

    rapidjson::Document doc;
    doc.Parse(line.c_str());
    REQUIRE_FALSE(doc.HasParseError());
    REQUIRE(std::string(doc["jsonrpc"].GetString()) == "2.0");
    REQUIRE(std::string(doc["method"].GetString()) == "event.app_exited");
    REQUIRE(doc["params"]["uid"].GetUint() == 0xE7351C2F);
    REQUIRE_FALSE(doc.HasMember("id"));
}

TEST_CASE("a_failed_auth_ends_the_session", "[control][dispatcher]") {
    dispatcher_fixture fixture;
    fixture.rpc.require_token("s3cret");

    // One wrong token per connection: the rest of the batch is refused, the right token too.
    rapidjson::Document response = fixture.call(R"([
        {"jsonrpc":"2.0","id":1,"method":"auth","params":{"token":"wrong"}},
        {"jsonrpc":"2.0","id":2,"method":"auth","params":{"token":"s3cret"}},
        {"jsonrpc":"2.0","id":3,"method":"echo"}
    ])");

    REQUIRE(response.IsArray());
    REQUIRE(response.Size() == 3);
    require_error(response[0], error_unauthorized);
    require_error(response[1], error_unauthorized);
    require_error(response[2], error_unauthorized);
    REQUIRE(fixture.client.rejected);
    REQUIRE_FALSE(fixture.client.authenticated);
    REQUIRE_FALSE(fixture.rpc.is_authorized(fixture.client));

    response = fixture.call(R"({"jsonrpc":"2.0","id":4,"method":"auth","params":{"token":"s3cret"}})");
    require_error(response, error_unauthorized);
    REQUIRE_FALSE(fixture.client.authenticated);
    REQUIRE(fixture.echo_calls == 0);
}

TEST_CASE("a_session_is_authorized_by_auth_or_when_no_token_is_required", "[control][dispatcher]") {
    dispatcher_fixture fixture;
    REQUIRE(fixture.rpc.is_authorized(fixture.client));

    fixture.rpc.require_token("s3cret");
    REQUIRE_FALSE(fixture.rpc.is_authorized(fixture.client));

    fixture.call(R"({"jsonrpc":"2.0","id":1,"method":"auth","params":{"token":"s3cret"}})");
    REQUIRE(fixture.rpc.is_authorized(fixture.client));
}

// JSON-RPC 2.0 section 7: an invalid request is answered with id null even when it has no id.
TEST_CASE("invalid_requests_without_id_are_answered", "[control][dispatcher]") {
    dispatcher_fixture fixture;

    rapidjson::Document response = fixture.call(R"({"jsonrpc":"2.0","method":1,"params":"bar"})");
    require_error(response, error_invalid_request);
    REQUIRE(response["id"].IsNull());

    response = fixture.call(R"({"foo":"boo"})");
    require_error(response, error_invalid_request);
    REQUIRE(response["id"].IsNull());

    response = fixture.call(R"([
        {"jsonrpc":"2.0","method":"echo","params":{"n":1}},
        {"jsonrpc":"2.0","method":"echo","params":{"n":2},"id":"2"},
        {"foo":"boo"},
        {"jsonrpc":"2.0","method":"nope","params":{"name":"myself"},"id":"5"},
        {"jsonrpc":"2.0","method":"echo","params":{"n":4},"id":"9"}
    ])");

    REQUIRE(response.IsArray());
    REQUIRE(response.Size() == 4);
    REQUIRE(std::string(response[0]["id"].GetString()) == "2");
    require_error(response[1], error_invalid_request);
    REQUIRE(response[1]["id"].IsNull());
    require_error(response[2], error_method_not_found);
    REQUIRE(std::string(response[3]["id"].GetString()) == "9");
    REQUIRE(fixture.echo_calls == 3);

    response = fixture.call("[1,2,3]");
    REQUIRE(response.IsArray());
    REQUIRE(response.Size() == 3);
    require_error(response[2], error_invalid_request);
}

namespace {
    // Handle a message on a thread of its own, as the server does: its stack is all a
    // recursive parser would have.
    std::string handle_on_a_thread(dispatcher_fixture &fixture, const std::string &line) {
        std::string response;
        std::thread worker([&]() {
            response = fixture.rpc.handle(fixture.client, line);
        });
        worker.join();

        return response;
    }
}

TEST_CASE("deep_nesting_is_a_parse_error_not_a_crash", "[control][dispatcher]") {
    dispatcher_fixture fixture;
    fixture.rpc.require_token("s3cret");

    for (const std::size_t depth : { std::size_t(1000000), std::size_t(4000000) }) {
        INFO("depth " << depth);

        rapidjson::Document response;
        response.Parse(handle_on_a_thread(fixture, std::string(depth, '[')).c_str());
        REQUIRE_FALSE(response.HasParseError());
        require_error(response, error_parse);
    }

    // Well-formed but deep: a batch whose only member is not a request.
    const std::size_t depth = 2000000;
    rapidjson::Document response;
    response.Parse(handle_on_a_thread(fixture, std::string(depth, '[') + std::string(depth, ']')).c_str());
    REQUIRE_FALSE(response.HasParseError());
    REQUIRE(response.IsArray());
    REQUIRE(response.Size() == 1);
    require_error(response[0], error_invalid_request);

    // Deep parameters are never walked by a method that does not read them.
    const std::string deep_params = std::string(depth, '[') + std::string(depth, ']');
    const std::string call = R"({"jsonrpc":"2.0","id":1,"method":"auth","params":{"token":"s3cret","x":)" + deep_params + "}}";
    response.Parse(handle_on_a_thread(fixture, call).c_str());
    REQUIRE_FALSE(response.HasParseError());
    REQUIRE(response["result"].IsObject());
}
