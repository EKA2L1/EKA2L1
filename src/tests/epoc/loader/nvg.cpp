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

// Path decoding in the NVG-to-SVG converter. Each case is built as a minimal
// direct-command NVG file holding a single filled path, because the interesting
// behaviour is entirely in how the path segment list and its coordinates are
// read. The shapes come from real Symbian^3 firmware icons.

#include <catch2/catch.hpp>
#include <loader/nvg.h>

#include <common/buffer.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace eka2l1;

namespace {
    // Offsets the converter reads the header at.
    constexpr std::size_t NVG_HEADER_SIZE = 52;
    constexpr std::size_t OFF_VERSION = 3;
    constexpr std::size_t OFF_HEADER_SIZE = 4;
    constexpr std::size_t OFF_PATH_DATATYPE = 26;
    constexpr std::size_t OFF_VIEWPORT = 36;

    constexpr std::uint16_t PATH_SIXTEEN_BIT = 2;
    constexpr std::uint16_t PATH_THIRTYTWO_BIT = 3;

    // VGPathSegment values, already shifted: bit 0 is the absolute/relative flag.
    constexpr std::uint8_t SEG_CLOSE_PATH = 0 << 1;
    constexpr std::uint8_t SEG_MOVE_TO = 1 << 1;
    constexpr std::uint8_t SEG_SCWARC_TO = 10 << 1;
    constexpr std::uint8_t SEG_RELATIVE = 1;

    void put16(std::vector<std::uint8_t> &buf, const std::size_t at, const std::uint16_t value) {
        buf[at] = static_cast<std::uint8_t>(value & 0xFF);
        buf[at + 1] = static_cast<std::uint8_t>(value >> 8);
    }

    void append16(std::vector<std::uint8_t> &buf, const std::int16_t value) {
        const std::uint16_t raw = static_cast<std::uint16_t>(value);

        buf.push_back(static_cast<std::uint8_t>(raw & 0xFF));
        buf.push_back(static_cast<std::uint8_t>(raw >> 8));
    }

    void append32(std::vector<std::uint8_t> &buf, const std::int32_t value) {
        const std::uint32_t raw = static_cast<std::uint32_t>(value);

        for (int i = 0; i < 4; i++) {
            buf.push_back(static_cast<std::uint8_t>((raw >> (i * 8)) & 0xFF));
        }
    }

    // Builds a direct-command NVG file with one DRAW_PATH command. Version 1, so
    // neither the command block nor the offset vector needs the version 2 padding.
    //
    //   0   signature, version, header size, type
    //   26  path data type
    //   36  viewport (x, y, w, h)
    //   52  offset vector: count, then one offset pointing at the path data
    //   56  command block: count, then the DRAW_PATH command
    //   62  path data: segment count, segment types, padding, coordinates
    std::vector<std::uint8_t> make_nvg(const std::vector<std::uint8_t> &segments,
        const std::vector<std::int32_t> &coords, const std::uint16_t datatype) {
        std::vector<std::uint8_t> buf(NVG_HEADER_SIZE, 0);

        std::memcpy(buf.data(), "nvg", 3);
        buf[OFF_VERSION] = 1;
        put16(buf, OFF_HEADER_SIZE, static_cast<std::uint16_t>(NVG_HEADER_SIZE));
        put16(buf, OFF_PATH_DATATYPE, datatype);

        const float viewport[4] = { 0.0f, 0.0f, 96.0f, 96.0f };
        std::memcpy(buf.data() + OFF_VIEWPORT, viewport, sizeof(viewport));

        // Offset vector: one entry, filled in once the data position is known.
        buf.resize(NVG_HEADER_SIZE + 4, 0);
        put16(buf, NVG_HEADER_SIZE, 1);

        // Command block: one DRAW_PATH (opcode 7) with the fill bit set, reading
        // entry 0 of the offset vector.
        const std::uint32_t command = (7u << 24) | 0x00020000u;
        buf.resize(NVG_HEADER_SIZE + 6, 0);
        put16(buf, NVG_HEADER_SIZE + 4, 1);

        for (int i = 0; i < 4; i++) {
            buf.push_back(static_cast<std::uint8_t>((command >> (i * 8)) & 0xFF));
        }

        put16(buf, NVG_HEADER_SIZE + 2, static_cast<std::uint16_t>(buf.size()));

        append16(buf, static_cast<std::int16_t>(segments.size()));
        buf.insert(buf.end(), segments.begin(), segments.end());

        // The converter aligns to the coordinate size before reading them.
        const std::size_t align = (datatype == PATH_THIRTYTWO_BIT) ? 4 : 2;
        while ((buf.size() % align) != 0) {
            buf.push_back(0);
        }

        for (const std::int32_t coord : coords) {
            if (datatype == PATH_THIRTYTWO_BIT) {
                append32(buf, coord);
            } else {
                append16(buf, static_cast<std::int16_t>(coord));
            }
        }

        return buf;
    }

    std::string convert(std::vector<std::uint8_t> nvg) {
        common::ro_buf_stream in(nvg.data(), nvg.size());

        std::vector<std::uint8_t> out_buf(64 * 1024, 0);
        common::wo_buf_stream out(out_buf.data(), out_buf.size());

        std::vector<loader::nvg_convert_error_description> errors;
        REQUIRE(loader::convert_nvg_to_svg(in, out, errors));

        return std::string(reinterpret_cast<const char *>(out_buf.data()), static_cast<std::size_t>(out.tell()));
    }

    std::size_t count_of(const std::string &haystack, const char needle) {
        std::size_t total = 0;

        for (const char c : haystack) {
            if (c == needle) {
                total++;
            }
        }

        return total;
    }
}

TEST_CASE("nvg_path_coordinates_decode_as_signed", "nvg_file") {
    // S11.4 fixed-point, so -352 is -22. Read unsigned it becomes 65184, i.e.
    // 4074 after scaling -- a coordinate far outside the 96x96 viewBox, which is
    // what left firmware icons truncated or blank.
    const std::string svg = convert(make_nvg({ SEG_MOVE_TO }, { -352, 16 }, PATH_SIXTEEN_BIT));

    REQUIRE(svg.find("M -22 1") != std::string::npos);
    REQUIRE(svg.find("4074") == std::string::npos);
}

TEST_CASE("nvg_path_coordinates_decode_as_signed_at_32_bit", "nvg_file") {
    // The same for S15.16, which is a separate instantiation of the decoder.
    const std::string svg = convert(make_nvg({ SEG_MOVE_TO }, { -22 * 65536, 65536 }, PATH_THIRTYTWO_BIT));

    REQUIRE(svg.find("M -22 1") != std::string::npos);
    REQUIRE(svg.find("65514") == std::string::npos);
}

TEST_CASE("nvg_close_path_does_not_end_the_path", "nvg_file") {
    // A single VGPath routinely holds several closed contours -- glyph outlines,
    // holes, multi-part shapes. Treating the first VG_CLOSE_PATH as the end of
    // the segment list dropped everything behind it.
    const std::vector<std::uint8_t> segments = { SEG_MOVE_TO, SEG_CLOSE_PATH, SEG_MOVE_TO };
    const std::string svg = convert(make_nvg(segments, { 16, 32, 48, 64 }, PATH_SIXTEEN_BIT));

    REQUIRE(count_of(svg, 'M') == 2);
    REQUIRE(svg.find("M 1 2") != std::string::npos);
    REQUIRE(svg.find("M 3 4") != std::string::npos);
}

TEST_CASE("nvg_small_clockwise_arc_is_a_known_segment", "nvg_file") {
    // VG_SCWARC_TO was missing from the command table (VG_SCCWARC_TO was listed
    // twice), so it was reported as an unknown segment and skipped -- without
    // consuming its five coordinates. Every segment after it then read the
    // wrong values, which is worse than losing the arc alone: here the move
    // that follows would pick up the arc's first two coordinates instead.
    const std::vector<std::uint8_t> segments = { SEG_SCWARC_TO, SEG_MOVE_TO };
    const std::string svg = convert(make_nvg(segments, { 16, 16, 0, 32, 48, 64, 80 }, PATH_SIXTEEN_BIT));

    // Small arc, so the large-arc flag is 0; clockwise, so the sweep flag is 1.
    REQUIRE(svg.find("A 1 1, 0, 0 1, 2 3") != std::string::npos);
    REQUIRE(svg.find("M 4 5") != std::string::npos);
}

TEST_CASE("nvg_relative_segment_is_dispatched_by_its_base_command", "nvg_file") {
    // Bit 0 marks a relative segment. Switching on the raw value sent every one
    // of them to the unreachable default, which abandons the whole path.
    const std::string svg = convert(make_nvg({ SEG_MOVE_TO | SEG_RELATIVE }, { 16, 32 }, PATH_SIXTEEN_BIT));

    REQUIRE(svg.find("m 1 2") != std::string::npos);
}
