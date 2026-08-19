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

#include <services/domain/domain.h>

using namespace eka2l1;

// The client derives the key it subscribes to on its own side, so the server has to
// arrive at exactly the same number or the subscription silently watches nothing.
// Values below are Symbian's own, from domaindefs.h:
//
//   KDmHierarchyIdPower = 1, KDmHierarchyIdStartup = 2
//   KDmIdRoot = 0x01, KDmIdApps = 0x02, KDmIdUiApps = 0x03
//
// and the derivation is DmStatePropertyKey() in domainsrv.h:
//
//   (aHierarchyId << 8) | ((aDomainId << 8) & 0xff0000) | (aDomainId & 0xff)

TEST_CASE("domain_state_property_key_matches_symbian", "domain") {
    // The power hierarchy's root domain, which is what a client joining for power
    // state notifications lands on.
    REQUIRE(epoc::dm::state_property_key(1, 0x01) == 0x000101);
    REQUIRE(epoc::dm::state_property_key(2, 0x02) == 0x000202);
    REQUIRE(epoc::dm::state_property_key(2, 0x03) == 0x000203);

    // A domain id is *not* contiguous in the key: its low byte stays at bit 0 and
    // its second byte moves to bit 16, straddling the hierarchy id. Anyone
    // "simplifying" this to a single shift breaks every two-byte domain id, and
    // nothing below 0x100 would notice.
    REQUIRE(epoc::dm::state_property_key(1, 0x0201) == 0x020101);
    REQUIRE(epoc::dm::state_property_key(0xFF, 0xFFFF) == 0xFFFFFF);

    // Bits above the domain's second byte are dropped rather than colliding with
    // the hierarchy id.
    REQUIRE(epoc::dm::state_property_key(1, 0x00FF0000) == 0x000100);
}

TEST_CASE("domain_state_property_value_matches_symbian", "domain") {
    // DmStatePropertyValue(): the transition id owns the top byte, the state the
    // rest, and DmStateFromPropertyValue() reads the state back with & 0xffffff.
    REQUIRE(epoc::dm::state_property_value(0, epoc::dm::POWER_STATE_ACTIVE) == 0);
    REQUIRE(epoc::dm::state_property_value(1, epoc::dm::POWER_STATE_ACTIVE) == 0x01000000);
    REQUIRE(epoc::dm::state_property_value(0xFF, 0xFFFFFF) == -1);

    // A state wider than 24 bits is truncated, not allowed to overwrite the id.
    REQUIRE(epoc::dm::state_property_value(0x12, 0xFF345678) == 0x12345678);
}
