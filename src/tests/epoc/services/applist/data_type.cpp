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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>
#include <services/applist/applist.h>
#include <services/applist/op.h>

using namespace eka2l1;

namespace {
    apa_app_registry handler(epoc::uid uid, std::initializer_list<data_type> types) {
        apa_app_registry registry;
        registry.mandatory_info.uid = uid;
        registry.data_types = types;
        return registry;
    }
}

TEST_CASE("app_for_data_type_uses_the_sdk_packet_and_opcode", "applist") {
    REQUIRE(sizeof(applist_data_type) == 268);
    REQUIRE(applist_request_app_for_data_type == 14);
}

TEST_CASE("app_for_data_type_selects_registered_priority_or_null", "applist") {
    const std::vector<apa_app_registry> apps = {
        handler(1, {{-20000, "image/png"}}),
        handler(2, {{10000, "image/png"}}),
        handler(3, {{0, "image/png"}, {10000, "text/plain"}})
    };
    REQUIRE(find_data_type_handler(apps, "image/png") == 2);
    REQUIRE(find_data_type_handler(apps, "text/plain") == 3);
    REQUIRE(find_data_type_handler(apps, "application/unknown") == 0);
    REQUIRE(find_data_type_handler(apps, "x-epoc/x-app12345678", 0x12345678) == 0x12345678);
    REQUIRE(find_data_type_handler(apps, "image/png", 0x12345678) == 2);
}

TEST_CASE("app_for_data_type_matches_apparc_wildcard_priorities", "applist") {
    // CApaAppData::DataType lowers wildcard priority; system priority is reserved.
    std::vector<apa_app_registry> apps = {
        handler(1, {{10000, "image/*"}}), handler(2, {{10000, "image/png"}})
    };
    REQUIRE(find_data_type_handler(apps, "image/png") == 2);
    REQUIRE(find_data_type_handler(apps, "image/jpeg") == 1);
    REQUIRE(find_data_type_handler({handler(3, {{0, "image/png"}})}, "image/*") == 3);
    apps.insert(apps.begin(), handler(4, {{0xFFF9, "image/*"}}));
    apps.push_back(handler(5, {{0xFFF9, "image/png"}}));
    REQUIRE(find_data_type_handler(apps, "image/png") == 4);
    REQUIRE(find_data_type_handler({handler(6, {{0, "image/p?g"}})}, "image/png") == 6);
    REQUIRE(find_data_type_handler({handler(7, {{0, "application/*+xml"}})}, "application/atom+xml") == 7);
    REQUIRE(find_data_type_handler({handler(7, {{0, "application/*+xml"}})}, "application/atomxml") == 0);
}
