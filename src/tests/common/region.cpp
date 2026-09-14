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

namespace {
    bool covers(const rect &area, int x, int y) {
        return x >= area.top.x && x < area.bottom_right().x
            && y >= area.top.y && y < area.bottom_right().y;
    }

    int coverage(const common::region &area, int x, int y) {
        int count = 0;
        for (const auto &part : area.rects_) {
            count += covers(part, x, y);
        }
        return count;
    }
}

TEST_CASE("Region union and subtraction preserve their pixel sets", "[region]") {
    const rect first({0, 0}, {5, 5});
    for (int x = -6; x <= 6; x++) {
        for (int y = -6; y <= 6; y++) {
            for (int width : {0, 1, 3, 8}) {
                for (int height : {0, 1, 3, 8}) {
                    const rect second({x, y}, {width, height});
                    common::region combined;
                    combined.add_rect(first);
                    combined.add_rect(second);
                    common::region removed;
                    removed.add_rect(first);
                    removed.eliminate(second);
                    bool union_matches = true;
                    bool subtraction_matches = true;
                    for (int px = -6; px < 14; px++) {
                        for (int py = -6; py < 14; py++) {
                            union_matches &= coverage(combined, px, py) == (covers(first, px, py) || covers(second, px, py));
                            subtraction_matches &= coverage(removed, px, py) == (covers(first, px, py) && !covers(second, px, py));
                        }
                    }
                    CAPTURE(x, y, width, height);
                    REQUIRE(union_matches);
                    REQUIRE(subtraction_matches);
                }
            }
        }
    }
}

TEST_CASE("Redraw invalidation retains the area outside overlapping controls", "[region]") {
    common::region invalid;
    invalid.add_rect(rect({0, 0}, {240, 322}));
    invalid.eliminate(rect({20, 76}, {54, 16}));
    invalid.add_rect(rect({7, 50}, {228, 49}));
    invalid.add_rect(rect({18, 144}, {208, 20}));

    bool matches = true;
    for (int x = 0; x < 240; x++) {
        for (int y = 0; y < 322; y++) {
            matches &= coverage(invalid, x, y) == 1;
        }
    }
    REQUIRE(matches);

    invalid.eliminate(rect({0, 0}, {240, 322}));
    REQUIRE(invalid.empty());
}

TEST_CASE("Region subtraction visits every existing rectangle", "[region]") {
    common::region area;
    area.add_rect(rect({0, 0}, {3, 3}));
    area.add_rect(rect({4, 0}, {3, 3}));
    area.add_rect(rect({8, 0}, {3, 3}));
    area.eliminate(rect({0, 1}, {11, 1}));
    for (int x = 0; x < 11; x++) {
        REQUIRE(coverage(area, x, 1) == 0);
    }
}
