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

#include <services/socket/server.h>

using namespace eka2l1;

// These are numbers on the wire: the guest's RSocketServ client sends the value,
// so renumbering the enum silently routes requests to the wrong handler.
//
// The reference is Symbian's own message table,
// esockserver/csock/SOCKMES.H (TESockMessages), quoted below by name and
// decimal value.

TEST_CASE("reformed_socket_opcodes_match_symbian_esock_message_table", "socket_opcodes") {
    // ESoRead = 39, ESoSetOpt = 49.
    REQUIRE(socket_reform_so_read == 0x27);
    REQUIRE(socket_reform_so_set_opt == 0x31);

    // ENDCreate = 66, ENDQuery = 67, ENDAdd = 68, ENDRemove = 69.
    REQUIRE(socket_reform_ndb_open == 0x42);
    REQUIRE(socket_reform_ndb_query == 0x43);
    REQUIRE(socket_reform_ndb_add == 0x44);
    REQUIRE(socket_reform_ndb_remove == 0x45);

    // ESoCreateWithConnection = 70 immediately follows ENDRemove, which pins the
    // end of the net database block.
    REQUIRE(socket_reform_so_open_with_conn == 0x46);

    // The cancel/close variants live in a separate block from 128 up:
    // ESoCancelAccept = 141, EHRCancel = 145, EHRClose = 146,
    // ENDCancel = 149, ENDClose = 150.
    REQUIRE(socket_reform_so_cancel_accept == 0x8D);
    REQUIRE(socket_reform_hr_cancel == 0x91);
    REQUIRE(socket_reform_hr_close == 0x92);
    REQUIRE(socket_reform_ndb_cancel == 0x95);
    REQUIRE(socket_reform_ndb_close == 0x96);
}

// The pre-S^3 table is not in any published header. It is recoverable anyway,
// because it runs the same subsession families in the same order as the table
// above, with two differences: there is no RecvOneOrMoreNoLength, and each
// family ends with its own Cancel and Close instead of collecting them into a
// separate block. Values EKA2L1 already serves real guests with bracket every
// value derived here, so each block is closed on both sides.

TEST_CASE("legacy_socket_read_sits_between_recv_one_or_more_and_write", "socket_opcodes") {
    // Order per SOCKMES.H: ESoRecv, ESoRecvNoLength, ESoRecvOneOrMore,
    // [ESoRecvOneOrMoreNoLength], ESoRead, ESoWrite. Dropping the bracketed one
    // leaves exactly one free slot between the two known values.
    REQUIRE(socket_so_recv_one_or_more == 0x0C);
    REQUIRE(socket_so_write == 0x0E);
    REQUIRE(socket_so_read == 0x0D);
}

TEST_CASE("legacy_host_resolver_block_is_contiguous_and_ends_in_cancel_close", "socket_opcodes") {
    // Order per SOCKMES.H: EHRCreate, EHRGetByName, EHRNext, EHRGetByAddress,
    // EHRGetHostName, EHRSetHostName. Open and GetByName anchor the start,
    // Close anchors the end.
    REQUIRE(socket_hr_open == 0x28);
    REQUIRE(socket_hr_get_by_name == 0x29);
    REQUIRE(socket_hr_close == 0x2F);

    REQUIRE(socket_hr_next == 0x2A);
    REQUIRE(socket_hr_get_by_address == 0x2B);
    REQUIRE(socket_hr_get_host_name == 0x2C);
    REQUIRE(socket_hr_set_host_name == 0x2D);

    // Cancel precedes Close in every family of the reformed table
    // (EHRCancel 145 / EHRClose 146, ESRCancel 147 / ESRClose 148,
    // ENDCancel 149 / ENDClose 150), which leaves 0x2E for Cancel.
    REQUIRE(socket_hr_cancel == 0x2E);
}

TEST_CASE("legacy_net_database_block_fills_the_gap_before_open_with_connection", "socket_opcodes") {
    // Between these two anchors sit the rest of the service resolver family
    // (RegisterService, RemoveService, Cancel, Close -- four slots, 0x33..0x36)
    // and then the whole net database family.
    REQUIRE(socket_sr_get_by_number == 0x32);
    REQUIRE(socket_so_open_with_connection == 0x3D);

    // ENDCreate, ENDQuery, ENDAdd, ENDRemove, then Cancel and Close: six slots,
    // 0x37..0x3C. The count closes the gap exactly, with no room to spare.
    REQUIRE(socket_ndb_open == 0x37);
    REQUIRE(socket_ndb_query == 0x38);
    REQUIRE(socket_ndb_add == 0x39);
    REQUIRE(socket_ndb_remove == 0x3A);
    REQUIRE(socket_ndb_cancel == 0x3B);
    REQUIRE(socket_ndb_close == 0x3C);
}
