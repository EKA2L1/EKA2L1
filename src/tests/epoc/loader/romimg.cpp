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

#include <common/buffer.h>
#include <config/config.h>
#include <loader/romimage.h>
#include <mem/mem.h>

#include <cstring>
#include <vector>

using namespace eka2l1;

namespace {
    // TRomImageHeader is 120 bytes on EKA2 and 100 bytes on EKA1.
    constexpr std::uint32_t EKA2_HEADER_SIZE = 120;

    constexpr std::uint32_t CODE_ADDRESS = 0xFC2C1618;
    constexpr std::uint32_t EXPORT_COUNT = 3;
    constexpr std::uint32_t EXPORTS[EXPORT_COUNT] = { 0xFC2C1700, 0xFC2C1740, 0xFC2C17C0 };

    void put32(std::vector<std::uint8_t> &image, const std::size_t offset, const std::uint32_t value) {
        std::memcpy(image.data() + offset, &value, sizeof(value));
    }

    std::vector<std::uint8_t> make_rom_image(const std::uint32_t header_size, const std::uint32_t export_dir_offset) {
        std::vector<std::uint8_t> image(header_size + 0x200, 0);

        put32(image, 0, 0x10000079);                            // uid1: a ROM DLL
        put32(image, 20, CODE_ADDRESS);                         // entry point
        put32(image, 24, CODE_ADDRESS);                         // code address
        put32(image, 60, EXPORT_COUNT);                         // export directory count
        put32(image, 64, CODE_ADDRESS + export_dir_offset);     // export directory address

        for (std::uint32_t i = 0; i < EXPORT_COUNT; i++) {
            put32(image, header_size + export_dir_offset + i * 4, EXPORTS[i]);
        }

        return image;
    }
}

TEST_CASE("rom_image_export_table_is_read_from_the_file_when_it_is_not_mapped", "loader") {
    config::state conf;
    memory_system mem(nullptr, &conf, mem::mem_model_type::multiple, false);

    const std::vector<std::uint8_t> image = make_rom_image(EKA2_HEADER_SIZE, 0x100);

    common::ro_buf_stream stream(const_cast<std::uint8_t *>(image.data()), image.size());
    std::optional<loader::romimg> parsed = loader::parse_romimg(&stream, &mem, epocver::epoc91);

    REQUIRE(parsed.has_value());
    REQUIRE(parsed->header.code_address == CODE_ADDRESS);
    REQUIRE(parsed->exports.size() == EXPORT_COUNT);

    for (std::uint32_t i = 0; i < EXPORT_COUNT; i++) {
        REQUIRE(parsed->exports[i] == EXPORTS[i]);
    }
}

TEST_CASE("rom_image_header_is_the_same_size_from_9_1_onwards", "loader") {
    REQUIRE(loader::rom_image_header_file_size(epocver::epoc91) == 120);
    REQUIRE(loader::rom_image_header_file_size(epocver::epoc93fp1) == 120);
    REQUIRE(loader::rom_image_header_file_size(epocver::epoc93fp2) == 120);
    REQUIRE(loader::rom_image_header_file_size(epocver::epoc10) == 120);
    REQUIRE(loader::rom_image_header_file_size(epocver::epoc80) == 100);
    REQUIRE(loader::rom_image_header_file_size(epocver::epoc81a) == 100);
}
