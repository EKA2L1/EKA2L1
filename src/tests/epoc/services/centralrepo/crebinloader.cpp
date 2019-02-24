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
#include <epoc/services/centralrepo/cre.h>

#include <common/chunkyseri.h>

#include <fstream>

using namespace eka2l1;

/* Behavior:
 *
 * ROM repo when modified will be stored in a prefer driver (internal if there is one) as binary (2008 forwards)
 * If that drive is not available, it will search for others. Repo ROM modified stored in Private folder/persists
 * 
 * New repo which not based on a ROM one will be stored in the Private folder of a modifiable drive
*/

TEST_CASE("generic_cre_loading", "centralrepo") {
    central_repo repo;
    std::ifstream fi("centralrepoassets/101f876f.cre", std::ios::binary | std::ios::ate);

    // Read to buffer
    std::vector<char> buf;
    buf.resize(fi.tellg());

    fi.seekg(0, std::ios::beg);
    fi.read(&buf[0], buf.size());

    common::chunkyseri seri(reinterpret_cast<std::uint8_t *>(&buf[0]), buf.size(), common::SERI_MODE_READ);
    do_state_for_cre(seri, repo);

    REQUIRE(repo.uid == 0x101F876F);
    REQUIRE(repo.entries.size() == 19);
    REQUIRE(repo.single_policies.size() == 19);
}