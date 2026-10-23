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
#include <kernel/svc.h>

using namespace eka2l1;

TEST_CASE("symbian3_flash_support_svcs_use_the_sdk_numbers", "[kernel_svc]") {
    REQUIRE(epoc::svc_register_funcs_v10.at(0x79).name == "process_rename");
    REQUIRE(epoc::svc_register_funcs_v10.at(0x5B).name == "set_exception_handler");
}

TEST_CASE("symbian91_locked_atomics_use_the_rom_fast_exec_numbers", "[kernel_svc]") {
    const auto &symbian91 = epoc::svc_register_funcs_v91_diff;
    REQUIRE(symbian91.at(0x0080000A).name == "locked_inc_32");
    REQUIRE(symbian91.at(0x0080000B).name == "locked_dec_32");
    REQUIRE(epoc::svc_register_funcs_v10.at(0x0080000A).name == "debug_mask");
    REQUIRE(epoc::svc_register_funcs_v10.at(0x0080000B).name == "debug_mask_index");
}
