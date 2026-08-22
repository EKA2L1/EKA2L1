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

#include <common/types.h>
#include <kernel/ipc.h>

using namespace eka2l1;

// EKA2 shipped in Symbian OS 8.1b. Everything up to and including 8.1a runs
// EKA1, which is why epocver puts epoc7, epoc80 and epoc81a below the eka2
// marker. The legacy IPC client ABI is the older boundary: it is what
// is_ipc_old() answers for, and it stops at epoc7.
//
// The gap between the two is the whole reason session_send_eka1 cannot decide
// on kernel version. An EKA1 client always sends four words and no
// argument-type header, but for these three releases is_ipc_old() is false, so
// keying off it makes the kernel read a fifth word the client never wrote.

TEST_CASE("eka1_releases_exist_above_the_legacy_ipc_boundary", "ipc_arg") {
    // The EKA1 side of the marker, in release order.
    REQUIRE(epocver::epoc6 < epocver::epoc7);
    REQUIRE(epocver::epoc7 < epocver::epoc80);
    REQUIRE(epocver::epoc80 < epocver::epoc81a);
    REQUIRE(epocver::epoc81a < epocver::eka2);

    // ... and 8.1b, the first EKA2 release, on the other side of it.
    REQUIRE(epocver::eka2 < epocver::epoc81b);

    // So these three are EKA1 while already being at or past the legacy IPC
    // boundary: the window session_send_eka1 has to cover on its own.
    for (const epocver ver : { epocver::epoc7, epocver::epoc80, epocver::epoc81a }) {
        REQUIRE(ver < epocver::eka2);        // is_eka1()
        REQUIRE_FALSE(ver < epocver::epoc7); // is_ipc_old()
    }
}

TEST_CASE("legacy_ipc_args_carry_no_per_slot_types", "ipc_arg") {
    // What session_send_general puts in flag when told there is no header.
    ipc_arg legacy(1, 2, 3, 4, 0xFFFFFFFF);

    const ipc_arg_type first = legacy.get_arg_type(0);
    for (int slot = 1; slot < 4; slot++) {
        REQUIRE(legacy.get_arg_type(slot) == first);
    }

    // A real header does distinguish slots -- three bits each, low slot first.
    const int header = static_cast<int>(ipc_arg_type::handle)
        | (static_cast<int>(ipc_arg_type::des8) << 3)
        | (static_cast<int>(ipc_arg_type::desc16) << 6);
    ipc_arg typed(1, 2, 3, 4, header);

    REQUIRE(typed.get_arg_type(0) == ipc_arg_type::handle);
    REQUIRE(typed.get_arg_type(1) == ipc_arg_type::des8);
    REQUIRE(typed.get_arg_type(2) == ipc_arg_type::desc16);
    REQUIRE(typed.get_arg_type(3) == ipc_arg_type::unspecified);
}
