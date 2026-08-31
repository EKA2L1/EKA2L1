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

#include <services/ui/icon/ops.h>

using namespace eka2l1;

// These are opcodes on the wire: the guest's AknIconServer client sends the number,
// not the name. The session dispatches on them and answers some of them without
// doing any work (preserving icon data is free for a server that re-renders from the
// container file), so renumbering the enum would silently make those replies land on
// the wrong requests. Pin the values that S60 3.x and Symbian^3 send.
TEST_CASE("akn_icon_server_opcodes_match_the_wire_numbering", "iconops") {
    REQUIRE(akn_icon_server_retrieve_or_create_shared_icon == 0);
    REQUIRE(akn_icon_server_free_bitmap == 1);
    REQUIRE(akn_icon_server_get_content_dim == 2);
    REQUIRE(akn_icon_server_preserve_icon_data == 3);
    REQUIRE(akn_icon_server_destroy_icon_data == 4);
    REQUIRE(akn_icon_server_get_init_data == 5);
    REQUIRE(akn_icon_server_request_to_enable_cache == 6);
}
