/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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
#include <common/cvt.h>
#include <services/centralrepo/centralrepo.h>

#include <iostream>

using namespace eka2l1;

static std::string ucs2_setting_value(const std::string &value) {
    const std::u16string value16 = common::utf8_to_ucs2(value);
    return std::string(reinterpret_cast<const char *>(value16.data()), value16.length() * 2);
}

TEST_CASE("ini_loader_no_default_meta_range", "centralrepo") {
    central_repo repo;

    REQUIRE(parse_new_centrep_ini("centralrepoassets/EFFF0000.ini", repo));
    REQUIRE(repo.entries.size() == 3);

    central_repo_entry *e1 = repo.find_entry(12);
    central_repo_entry *e2 = repo.find_entry(13);
    central_repo_entry *e3 = repo.find_entry(78);

    REQUIRE(e1);
    REQUIRE(e2);
    REQUIRE(e3);

    REQUIRE(e2->data.etype == central_repo_entry_type::real);
    REQUIRE(e2->data.reald == 5.7);

    REQUIRE(e1->data.etype == central_repo_entry_type::integer);
    REQUIRE(e1->data.intd == 15);
    REQUIRE(e3->metadata_val == 12);
}

TEST_CASE("ini_loader_meta_default_and_range", "centralrepo") {
    central_repo repo;

    REQUIRE(parse_new_centrep_ini("centralrepoassets/EFFF0001.ini", repo));
    REQUIRE(repo.entries.size() == 2);

    central_repo_entry *e1 = repo.find_entry(5);
    central_repo_entry *e2 = repo.find_entry(0x42);

    REQUIRE(e1);
    REQUIRE(e2);

    REQUIRE(e1->metadata_val == 10);
    REQUIRE(e2->metadata_val == 12);
}

TEST_CASE("ini_loader_all_value_types", "centralrepo") {
    central_repo repo;

    REQUIRE(parse_new_centrep_ini("centralrepoassets/EFFF0002.ini", repo));
    REQUIRE(repo.owner_uid == 0x10203040);
    REQUIRE(repo.entries.size() == 12);

    // A negative integer keeps its sign.
    REQUIRE(static_cast<std::int32_t>(repo.find_entry(0x1)->data.intd) == -1);
    REQUIRE(repo.find_entry(0x2)->data.intd == 0x20);

    // An empty quoted string is a value, not a missing one.
    REQUIRE(repo.find_entry(0x3)->data.etype == central_repo_entry_type::string);
    REQUIRE(repo.find_entry(0x3)->data.strd.empty());

    REQUIRE(repo.find_entry(0x4)->data.strd == ucs2_setting_value("quoted, with separators"));
    REQUIRE(repo.find_entry(0x5)->data.strd == "utf8");

    // '-' is how the format spells empty binary data.
    REQUIRE(repo.find_entry(0x6)->data.strd.empty());
    REQUIRE(repo.find_entry(0x7)->data.strd == std::string("\x0A\x0B", 2));

    REQUIRE(repo.find_entry(0x8)->data.reald == 5.7);
    REQUIRE(repo.find_entry(0x8)->metadata_val == 9);

    // Escapes are expanded, and the access policies after the metadata are not values.
    REQUIRE(repo.find_entry(0x9)->data.strd == ucs2_setting_value("escaped \"quote\""));
    REQUIRE(repo.find_entry(0x9)->metadata_val == 7);

    // Metadata that the settings do not carry comes from the defaults.
    REQUIRE(repo.find_entry(0x3)->metadata_val == 0x01000000);
    REQUIRE(repo.find_entry(0x40)->metadata_val == 12);
    REQUIRE(repo.find_entry(0x1001)->metadata_val == 34);
    REQUIRE(repo.find_entry(0x2001)->metadata_val == 56);
}
