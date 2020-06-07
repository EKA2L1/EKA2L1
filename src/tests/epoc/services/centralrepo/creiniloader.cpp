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
#include <services/centralrepo/centralrepo.h>

#include <iostream>

using namespace eka2l1;

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

    REQUIRE(e2->data.intd == 15);
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