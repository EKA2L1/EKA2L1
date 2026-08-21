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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>
#include <common/sync.h>

using namespace eka2l1;

TEST_CASE("event wait atomically consumes one signal", "[common][sync]") {
    common::event evt;

    evt.set();
    evt.wait();

    REQUIRE_FALSE(evt.wait_for(1000));

    evt.set();
    REQUIRE(evt.wait_for(100000));
    REQUIRE_FALSE(evt.wait_for(1000));
}
