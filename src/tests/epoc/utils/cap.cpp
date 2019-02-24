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
#include <epoc/utils/cap.h>

using namespace eka2l1;

/**
 * Policy has 1 capability: SWEvent.
 * Security info has 2 capabilities: SWEvent and AllFiles.
 * 
 * The test expect the security info to pass the policy check.
 */
TEST_CASE("policy_c1_vs_info_c2_pass", "capabilities") {
    epoc::security_policy sw_policy({ epoc::cap_sw_event });
    epoc::security_info   process_sec_info({ epoc::cap_all_files, epoc::cap_sw_event });
    epoc::security_info   missing;

    REQUIRE(sw_policy.check(process_sec_info, missing));
}

/**
 * Policy has 4 capabilities: CommDD, DiskAdmin, Location, Drm.
 * Security info has 2 capabilities: DiskAdmin, Location.
 * 
 * The test expect the security info to fail the policy check.
 * The missing struct must report back that CommDD and Drm is missing.
 */
TEST_CASE("policy_c4_vs_info_c2_fail", "capabilities") {
    epoc::security_policy policy({ epoc::cap_comm_dd, epoc::cap_disk_admin, epoc::cap_loc, 
        epoc::cap_drm });

    epoc::security_info   sec_info({ epoc::cap_disk_admin, epoc::cap_loc }); 
    epoc::security_info   missing;

    REQUIRE(policy.check(sec_info, missing) == false);
    REQUIRE(missing.caps.get(static_cast<std::size_t>(epoc::cap_comm_dd)));
    REQUIRE(missing.caps.get(static_cast<std::size_t>(epoc::cap_drm)));
}
