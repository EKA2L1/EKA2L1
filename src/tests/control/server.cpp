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

#include <config/app_settings.h>
#include <config/config.h>
#include <control/dispatcher.h>
#include <control/frontend.h>
#include <control/keys.h>
#include <control/server.h>
#include <system/epoc.h>

#include <rapidjson/document.h>

#include <cstdlib>
#include <memory>
#include <string>

using namespace eka2l1;
using namespace eka2l1::control;

namespace {
    struct fake_frontend : public frontend {
        eka2l1::system *sys = nullptr;
        bool paused = false;
        int exit_code = -1;

        eka2l1::system *get_system() override {
            return sys;
        }

        void pause() override {
            paused = true;
        }

        void resume() override {
            paused = false;
        }

        bool is_paused() override {
            return paused;
        }

        void request_exit(const int code) override {
            exit_code = code;
        }
    };

    // A system that started up but has no device: there is no emulation loop to run tasks.
    struct deviceless_system {
        config::state conf;
        config::app_settings settings;
        system_create_components components;
        std::unique_ptr<eka2l1::system> sys;

        explicit deviceless_system()
            : settings(&conf) {
            components.conf_ = &conf;
            components.settings_ = &settings;

            sys = std::make_unique<eka2l1::system>(components);
            sys->startup();
        }
    };

    struct server_fixture {
        fake_frontend host;
        control::server server;
        session client;

        explicit server_fixture()
            : server(host) {
        }

        rapidjson::Document call(const std::string &method, const std::string &params = "{}") {
            const std::string line = server.get_dispatcher().handle(client,
                R"({"jsonrpc":"2.0","id":1,"method":")" + method + R"(","params":)" + params + "}");

            rapidjson::Document doc;
            doc.Parse(line.c_str());
            REQUIRE_FALSE(doc.HasParseError());

            return doc;
        }

        int error_of(const std::string &method, const std::string &params = "{}") {
            rapidjson::Document response = call(method, params);
            INFO(method << " " << params);
            REQUIRE(response.HasMember("error"));
            return response["error"]["code"].GetInt();
        }
    };
}

TEST_CASE("emulator_info_works_without_a_system", "[control][server]") {
    server_fixture fixture;

    rapidjson::Document info = fixture.call("emulator.info");
    REQUIRE(std::string(info["result"]["name"].GetString()) == "EKA2L1");
    REQUIRE(info["result"]["protocol"].GetInt() == 1);
    REQUIRE(info["result"]["version"].IsString());
    REQUIRE(info["result"]["paused"].GetBool() == false);
    REQUIRE(info["result"]["device"].IsNull());
}

TEST_CASE("pause_resume_and_exit_go_to_the_frontend", "[control][server]") {
    server_fixture fixture;

    REQUIRE(fixture.call("emulator.pause")["result"]["paused"].GetBool());
    REQUIRE(fixture.host.paused);
    REQUIRE(fixture.call("emulator.info")["result"]["paused"].GetBool());

    REQUIRE_FALSE(fixture.call("emulator.resume")["result"]["paused"].GetBool());
    REQUIRE_FALSE(fixture.host.paused);

    REQUIRE(fixture.error_of("emulator.exit", R"({"code":300})") == error_invalid_params);
    REQUIRE(fixture.host.exit_code == -1);

    REQUIRE(fixture.call("emulator.exit", R"({"code":3})").HasMember("result"));
    REQUIRE(fixture.host.exit_code == 3);
}

TEST_CASE("guest_methods_say_when_there_is_no_system", "[control][server]") {
    server_fixture fixture;

    REQUIRE(fixture.error_of("apps.list") == error_not_ready);
    REQUIRE(fixture.error_of("app.launch", R"({"uid":"0xE7351C2F"})") == error_not_ready);
    REQUIRE(fixture.error_of("app.kill", R"({"uid":3879017519})") == error_not_ready);
    REQUIRE(fixture.error_of("input.key", R"({"key":"select"})") == error_not_ready);
    REQUIRE(fixture.error_of("input.touch", R"({"x":10,"y":20})") == error_not_ready);
    REQUIRE(fixture.error_of("screen.capture") == error_not_ready);
    REQUIRE(fixture.error_of("package.remove", R"({"uid":1})") == error_not_ready);
}

TEST_CASE("parameters_are_checked_before_the_guest_is_touched", "[control][server]") {
    server_fixture fixture;

    REQUIRE(fixture.error_of("app.launch") == error_invalid_params);
    REQUIRE(fixture.error_of("app.launch", R"({"uid":"launcher"})") == error_invalid_params);
    REQUIRE(fixture.error_of("input.key") == error_invalid_params);
    REQUIRE(fixture.error_of("input.key", R"({"key":"jump"})") == error_invalid_params);
    REQUIRE(fixture.error_of("input.key", R"({"key":"select","scancode":167})") == error_invalid_params);
    REQUIRE(fixture.error_of("input.key", R"({"key":"select","action":"hold"})") == error_invalid_params);
    REQUIRE(fixture.error_of("input.key", R"({"key":"select","action":"move"})") == error_invalid_params);
    REQUIRE(fixture.error_of("input.touch", R"({"x":10})") == error_invalid_params);
    REQUIRE(fixture.error_of("input.touch", R"({"x":10,"y":-1})") == error_invalid_params);
    REQUIRE(fixture.error_of("input.touch", R"({"x":10,"y":10,"pointer":8})") == error_invalid_params);
    REQUIRE(fixture.error_of("screen.capture", R"({"path":7})") == error_invalid_params);
    REQUIRE(fixture.error_of("package.install") == error_invalid_params);
    REQUIRE(fixture.error_of("package.install", R"({"path":"/nonexistent/app.sisx"})") == error_not_found);
}

TEST_CASE("guest_methods_run_without_an_emulation_loop", "[control][server]") {
    // No device has been booted, so nothing calls loop(): the requests run on the caller.
    deviceless_system guest;
    server_fixture fixture;
    fixture.host.sys = guest.sys.get();

    REQUIRE(fixture.error_of("apps.list") == error_not_ready);
    REQUIRE(fixture.error_of("input.key", R"({"key":"5"})") == error_not_ready);

    // The pause waits for the emulation thread to settle, which here means not at all.
    REQUIRE(fixture.call("emulator.pause")["result"]["paused"].GetBool());
}

TEST_CASE("tcp_needs_a_token_from_the_environment", "[control][server]") {
    fake_frontend host;
    control::server server(host);

#if defined(_WIN32)
    _putenv_s("EKA2L1_CONTROL_TOKEN", "");
#else
    unsetenv("EKA2L1_CONTROL_TOKEN");
#endif

    std::string error;
    REQUIRE_FALSE(server.start("tcp:127.0.0.1:1", error));
    REQUIRE(error.find("EKA2L1_CONTROL_TOKEN") != std::string::npos);

    REQUIRE_FALSE(server.start("tcp:10.0.0.1:5555", error));
    REQUIRE(error.find("loopback") != std::string::npos);
}

TEST_CASE("tcp_refuses_a_token_shorter_than_16_bytes", "[control][server]") {
    fake_frontend host;
    control::server server(host);

#if defined(_WIN32)
    _putenv_s("EKA2L1_CONTROL_TOKEN", "0123456789abcde");
#else
    setenv("EKA2L1_CONTROL_TOKEN", "0123456789abcde", 1);
#endif

    std::string error;
    REQUIRE_FALSE(server.start("tcp:127.0.0.1:1", error));
    REQUIRE(error.find("EKA2L1_CONTROL_TOKEN") != std::string::npos);
    REQUIRE(error.find("16") != std::string::npos);

#if defined(_WIN32)
    _putenv_s("EKA2L1_CONTROL_TOKEN", "");
#else
    unsetenv("EKA2L1_CONTROL_TOKEN");
#endif
}

TEST_CASE("keys_have_names_for_the_phone_keypad", "[control][keys]") {
    REQUIRE(scan_code_of_key("left_softkey") == 0xA4u);
    REQUIRE(scan_code_of_key("right_softkey") == 0xA5u);
    REQUIRE(scan_code_of_key("select") == 0xA7u);
    REQUIRE(scan_code_of_key("up") == 0x10u);
    REQUIRE(scan_code_of_key("down") == 0x11u);
    REQUIRE(scan_code_of_key("left") == 0x0Eu);
    REQUIRE(scan_code_of_key("right") == 0x0Fu);
    REQUIRE(scan_code_of_key("send") == 0xC4u);
    REQUIRE(scan_code_of_key("end") == 0xC5u);
    REQUIRE(scan_code_of_key("5") == static_cast<std::uint32_t>('5'));
    REQUIRE(scan_code_of_key("star") == static_cast<std::uint32_t>('*'));
    REQUIRE(scan_code_of_key("hash") == 0x7Fu);
    REQUIRE_FALSE(scan_code_of_key("55").has_value());
    REQUIRE_FALSE(scan_code_of_key("Select").has_value());
    REQUIRE_FALSE(scan_code_of_key("").has_value());
}
