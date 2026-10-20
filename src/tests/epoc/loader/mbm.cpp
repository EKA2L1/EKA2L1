/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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
#include <loader/mbm.h>

#include <common/buffer.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

using namespace eka2l1;

/**
 * Try to read headers from a MBM file. Test its results. 
 */
TEST_CASE("mbm_header_trailer_and_single_headers", "mbm_file") {
    std::ifstream fi("loaderassets/face.mbm", std::ios::binary);
    REQUIRE(!fi.bad());
    REQUIRE(!fi.fail());

    std::vector<std::uint8_t> data;

    fi.seekg(0, std::ios::end);
    const std::uint64_t fsize = fi.tellg();

    data.resize(fsize);
    fi.seekg(0, std::ios::beg);

    fi.read(reinterpret_cast<char *>(&data[0]), data.size());

    common::ro_buf_stream stream(&data[0], data.size());
    loader::mbm_file mbmf(reinterpret_cast<common::ro_stream *>(&stream));

    REQUIRE(mbmf.do_read_headers());
    REQUIRE(mbmf.trailer.count == 1);
    REQUIRE(mbmf.trailer.sbm_offsets[0] == 20);

    REQUIRE(mbmf.sbm_headers[0].bitmap_size == 3545);
    REQUIRE(mbmf.sbm_headers[0].bit_per_pixels == 24);
}

/**
 * A client can ask for a bitmap index the file does not have. The header read
 * must skip it rather than index the trailer out of bounds.
 */
TEST_CASE("mbm_out_of_range_index_is_skipped", "mbm_file") {
    std::ifstream fi("loaderassets/face.mbm", std::ios::binary);
    REQUIRE(!fi.bad());
    REQUIRE(!fi.fail());

    std::vector<std::uint8_t> data;

    fi.seekg(0, std::ios::end);
    const std::uint64_t fsize = fi.tellg();

    data.resize(fsize);
    fi.seekg(0, std::ios::beg);

    fi.read(reinterpret_cast<char *>(&data[0]), data.size());

    common::ro_buf_stream stream(&data[0], data.size());
    loader::mbm_file mbmf(reinterpret_cast<common::ro_stream *>(&stream));

    // face.mbm holds a single bitmap, so index 1 is one past the end.
    mbmf.index_to_loads = { 1 };

    REQUIRE(mbmf.do_read_headers());
    REQUIRE(mbmf.trailer.count == 1);
    REQUIRE(!mbmf.is_header_loaded(1));
}

/**
 * A ROM image MBM holds in-memory CBitwiseBitmaps, whose RLE compression types
 * are offset by 50. A byte RLE bitmap is therefore stored with compression 51.
 */
TEST_CASE("mbm_rom_image_rle_bitmap", "mbm_file") {
    const std::uint8_t rle[] = { 0x02, 0xAB, 0xFE, 0x12, 0x34 };

    std::vector<std::uint32_t> words = {
        0x10000041, 1, 12,                                       // UID, count, bitmap offset
        0x10000040, 1, 0, 0, 8,                                  // CBitwiseBitmap prefix, byte width 8
        0x28 + sizeof(rle), 0x28, 5, 1, 0, 0, 8, 1, 0, 51,       // SEpocBitmapHeader
        0xFFFFFFFF, 0x44                                         // spare, data offset
    };

    std::vector<std::uint8_t> data(words.size() * 4 + sizeof(rle));
    std::memcpy(data.data(), words.data(), words.size() * 4);
    std::memcpy(data.data() + words.size() * 4, rle, sizeof(rle));

    common::ro_buf_stream stream(&data[0], data.size());
    loader::mbm_file mbmf(reinterpret_cast<common::ro_stream *>(&stream));

    REQUIRE(mbmf.do_read_headers());
    REQUIRE(mbmf.sbm_headers[0].compression == 1);

    std::uint8_t pixels[5] = {};
    std::size_t pixels_size = sizeof(pixels);

    REQUIRE(mbmf.read_single_bitmap(0, pixels, pixels_size));
    REQUIRE(pixels_size == 5);

    const std::uint8_t expected[] = { 0xAB, 0xAB, 0xAB, 0x12, 0x34 };
    REQUIRE(std::memcmp(pixels, expected, sizeof(expected)) == 0);
}
