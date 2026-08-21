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
#include <common/region.h>

using namespace eka2l1;

TEST_CASE("clip_region_to_bounds", "region") {
    common::region region;
    region.add_rect(eka2l1::rect({ -10, -20 }, { 40, 50 }));

    region.clip(eka2l1::rect({ 0, 0 }, { 20, 15 }));

    REQUIRE(region.rects_.size() == 1);
    REQUIRE(region.rects_[0] == eka2l1::rect({ 0, 0 }, { 20, 15 }));
}

TEST_CASE("clip_region_removes_empty_rectangles", "region") {
    common::region region;
    region.add_rect(eka2l1::rect({ 0, 0 }, { 240, 236 }));

    region.clip(eka2l1::rect({ 0, 0 }, { 0, 0 }));

    REQUIRE(region.empty());
}
