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
#include <control/params.h>

#include <rapidjson/document.h>

#include <functional>

using namespace eka2l1::control;

namespace {
    rapidjson::Document params_of(const char *json) {
        rapidjson::Document doc;
        doc.Parse(json);
        REQUIRE(doc.IsObject());
        return doc;
    }

    int error_of(const std::function<void()> &read) {
        try {
            read();
        } catch (rpc_error &error) {
            return error.code();
        }

        return 0;
    }
}

TEST_CASE("uids_are_numbers_or_hexadecimal_strings", "[control][params]") {
    REQUIRE(require_uid(params_of(R"({"uid":3879017519})"), "uid") == 0xE7351C2F);
    REQUIRE(require_uid(params_of(R"({"uid":"0xE7351C2F"})"), "uid") == 0xE7351C2F);
    REQUIRE(require_uid(params_of(R"({"uid":"0xe7351c2f"})"), "uid") == 0xE7351C2F);

    for (const char *bad : { R"({})", R"({"uid":-1})", R"({"uid":4294967296})", R"({"uid":1.5})",
             R"({"uid":"E7351C2F"})", R"({"uid":"0x"})", R"({"uid":"0x-1"})", R"({"uid":"0x123456789"})",
             R"({"uid":"0xZZ"})", R"({"uid":true})" }) {
        INFO(bad);
        REQUIRE(error_of([&] { require_uid(params_of(bad), "uid"); }) == error_invalid_params);
    }
}

TEST_CASE("integers_are_range_checked", "[control][params]") {
    REQUIRE(require_integer(params_of(R"({"x":5})"), "x", 0, 10) == 5);
    REQUIRE_FALSE(optional_integer(params_of(R"({})"), "x", 0, 10).has_value());
    REQUIRE(error_of([] { require_integer(params_of(R"({"x":11})"), "x", 0, 10); }) == error_invalid_params);
    REQUIRE(error_of([] { require_integer(params_of(R"({"x":"5"})"), "x", 0, 10); }) == error_invalid_params);
    REQUIRE(error_of([] { require_integer(params_of(R"({})"), "x", 0, 10); }) == error_invalid_params);
}

TEST_CASE("strings_are_type_checked", "[control][params]") {
    REQUIRE(require_string(params_of(R"({"path":"/tmp/a.sis"})"), "path") == "/tmp/a.sis");
    REQUIRE_FALSE(optional_string(params_of(R"({})"), "path").has_value());
    REQUIRE(error_of([] { require_string(params_of(R"({"path":1})"), "path"); }) == error_invalid_params);
    REQUIRE(error_of([] { require_string(params_of(R"({})"), "path"); }) == error_invalid_params);
}
