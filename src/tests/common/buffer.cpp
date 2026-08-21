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
#include <common/buffer.h>

using namespace eka2l1;

TEST_CASE("ro_std_file_stream_over_missing_file", "buffer") {
    common::ro_std_file_stream stream("this-file-does-not-exist", true);
    char scratch[8] = {};

    REQUIRE_FALSE(stream.valid());
    REQUIRE(stream.read(scratch, sizeof(scratch)) == 0);
    REQUIRE(stream.size() == 0);
    REQUIRE(stream.tell() == 0);

    stream.seek(4, common::seek_where::beg);
    REQUIRE(stream.tell() == 0);
}
