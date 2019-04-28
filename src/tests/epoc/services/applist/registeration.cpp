/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <epoc/services/applist/applist.h>
#include <epoc/loader/rsc.h>
#include <epoc/vfs.h>

#include <common/algorithm.h>
#include <common/buffer.h>

#include <catch2/catch.hpp>

using namespace eka2l1;

TEST_CASE("mandatory_check_non_localise", "applist_registeration") {
    symfile f = eka2l1::physical_file_proxy("applistassets//sample_reg.rsc", READ_MODE | BIN_MODE);
    REQUIRE(f);

    eka2l1::ro_file_stream std_rsc_raw(f);
    REQUIRE(std_rsc_raw.valid());

    loader::rsc_file std_rsc(reinterpret_cast<common::ro_stream*>(&std_rsc_raw));
    auto dat = std_rsc.read(1);

    common::ro_buf_stream app_info_resource_stream(&dat[0], dat.size());
    apa_app_registry reg;

    bool result = read_registeration_info(reinterpret_cast<common::ro_stream*>(&app_info_resource_stream),
        reg, drive_c);

    REQUIRE(result);
    REQUIRE(common::compare_ignore_case(reg.mandatory_info.app_path.to_std_string(nullptr), 
        u"C:\\System\\Programs\\ITried_0xed3e09d5.exe") == 0);
    REQUIRE(reg.caps.flags == 0);
    REQUIRE(reg.caps.ability == apa_capability::embeddability::not_embeddable);
    REQUIRE(reg.default_screen_number == 0);
}
