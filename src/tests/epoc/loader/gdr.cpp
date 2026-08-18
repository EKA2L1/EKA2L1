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

// Header identification in the bitmap font store parser. A font file is offered
// to each rasterizer in turn until one recognises it, so the parser has to be
// able to say no to a file that is not a font store -- everything after the
// header is counts and offsets, and reading those out of unrelated bytes walks
// the parser off the end of the buffer.

#include <catch2/catch.hpp>
#include <loader/gdr.h>

#include <common/buffer.h>

#include <cstdint>
#include <vector>

using namespace eka2l1;

namespace {
    // fnttran writes these as literal constants at the head of every font store:
    // bitmapfonttools/src/FNTRECRD.CPP, FontStoreFile::ExternalizeHeader, with the
    // values in bitmapfonttools/inc/UID.H.
    constexpr std::uint32_t STORE_WRITE_ONCE_LAYOUT_UID = 268435511;
    constexpr std::uint32_t FONT_STORE_FILE_UID = 268435513;
    constexpr std::uint32_t NULL_UID = 0;
    constexpr std::uint32_t FONT_STORE_FILE_CHECKSUM = 0x47393853;

    void append32(std::vector<std::uint8_t> &buf, const std::uint32_t value) {
        for (int i = 0; i < 4; i++) {
            buf.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }

    // A font store holding no fonts: the identifying header, the five header
    // words that carry no constants, then an empty copyright list, an empty font
    // bitmap list and an empty typeface list.
    std::vector<std::uint8_t> make_empty_store(const std::uint32_t layout_uid = STORE_WRITE_ONCE_LAYOUT_UID,
        const std::uint32_t file_uid = FONT_STORE_FILE_UID,
        const std::uint32_t checksum = FONT_STORE_FILE_CHECKSUM) {
        std::vector<std::uint8_t> buf;

        append32(buf, layout_uid);
        append32(buf, file_uid);
        append32(buf, NULL_UID);
        append32(buf, checksum);

        append32(buf, 0); // id offset
        append32(buf, 0); // fnttran version
        append32(buf, 0); // collection uid
        append32(buf, 0); // pixel aspect ratio
        append32(buf, 0); // data stream id

        append32(buf, 0); // copyright strings
        append32(buf, 0); // font bitmaps
        append32(buf, 0); // typefaces

        return buf;
    }

    bool parse(std::vector<std::uint8_t> buf) {
        common::ro_buf_stream stream(buf.data(), buf.size());

        loader::gdr::file_store store;
        return loader::gdr::parse_store(reinterpret_cast<common::ro_stream *>(&stream), store);
    }
}

TEST_CASE("gdr_store_header_is_identified", "gdr_file") {
    REQUIRE(parse(make_empty_store()));
}

TEST_CASE("gdr_rejects_a_file_that_is_not_a_font_store", "gdr_file") {
    // What a .ttf, or anything else offered to this parser, looks like.
    REQUIRE(!parse(std::vector<std::uint8_t>(256, 0x5A)));
}

TEST_CASE("gdr_rejects_a_store_with_the_wrong_identifiers", "gdr_file") {
    // Each of the three constants on its own is enough to disqualify the file,
    // so a near-miss -- a printer store, which shares the layout uid -- cannot
    // be read as a font store.
    REQUIRE(!parse(make_empty_store(STORE_WRITE_ONCE_LAYOUT_UID + 1)));
    REQUIRE(!parse(make_empty_store(STORE_WRITE_ONCE_LAYOUT_UID, 268435514))); // KPdrStoreFileUid
    REQUIRE(!parse(make_empty_store(STORE_WRITE_ONCE_LAYOUT_UID, FONT_STORE_FILE_UID, 0x4739A38F))); // KPdrStoreFileChecksum
}

TEST_CASE("gdr_rejects_a_truncated_header", "gdr_file") {
    std::vector<std::uint8_t> buf = make_empty_store();
    buf.resize(10);

    REQUIRE(!parse(buf));
}
