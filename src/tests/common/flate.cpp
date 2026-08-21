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
#include <common/flate.h>

#include <memory>

using namespace eka2l1;

// TBitInput::Set takes a length in bits, and the stream it describes is only
// as long as that: Symbian's compressed sections are byte granular and are
// routinely not a whole number of words. Reading the last bits must therefore
// stay inside the buffer the caller supplied. Sized exactly so a sanitiser
// build traps an over-read.
TEST_CASE("bit_input stays inside a stream that is not word sized", "flate") {
    constexpr std::uint8_t stream[] = { 0x12, 0x34, 0x56, 0x78, 0x9A };
    std::unique_ptr<std::uint8_t[]> buffer = std::make_unique<std::uint8_t[]>(sizeof(stream));

    std::copy(stream, stream + sizeof(stream), buffer.get());

    flate::bit_input input;
    input.set(buffer.get(), static_cast<int>(sizeof(stream)) * 8);

    for (const std::uint8_t expected : stream) {
        REQUIRE(input.read(8) == expected);
    }
}
