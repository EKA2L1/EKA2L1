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

// Attribute and character-data decoding in the SVGB-to-SVG converter. Every case
// is a hand-built minimal SVGB stream, because what matters is only whether the
// decoder consumes exactly as many bytes as the binariser wrote: one byte read
// short or long desynchronises the stream and the remainder of the icon is lost,
// which is how these bugs presented.

#include <catch2/catch.hpp>
#include <loader/svgb.h>

#include <common/buffer.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace eka2l1;

namespace {
    // Stream markers, from the binary format the converter implements.
    constexpr std::uint32_t SVGB_MAGIC = 0x03FA56CC;
    constexpr std::uint8_t SVGB_FILE_END = 0xFF;
    constexpr std::uint8_t SVGB_ELEMENT_END = 0xFE;
    constexpr std::uint8_t SVGB_CDATA = 0xFD;
    constexpr std::uint16_t SVGB_ATTR_END = 1000;

    // Element codes, as indexed in the converter's element table.
    constexpr std::uint8_t ELEM_GROUP = 11;
    constexpr std::uint8_t ELEM_TEXT = 25;
    constexpr std::uint8_t ELEM_RECT = 33;

    // Attribute ids.
    constexpr std::uint16_t ATTR_Y = 48;
    constexpr std::uint16_t ATTR_X = 49;

    void append16(std::vector<std::uint8_t> &buf, const std::uint16_t value) {
        buf.push_back(static_cast<std::uint8_t>(value & 0xFF));
        buf.push_back(static_cast<std::uint8_t>(value >> 8));
    }

    void append32(std::vector<std::uint8_t> &buf, const std::uint32_t value) {
        for (int i = 0; i < 4; i++) {
            buf.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }

    void append_float(std::vector<std::uint8_t> &buf, const float value) {
        std::uint32_t raw = 0;
        std::memcpy(&raw, &value, sizeof(raw));

        append32(buf, raw);
    }

    // A length-prefixed string, the one shape the format uses for both character
    // data and string attributes: a single count byte, then that many bytes.
    void append_string(std::vector<std::uint8_t> &buf, const std::vector<std::uint8_t> &bytes) {
        buf.push_back(static_cast<std::uint8_t>(bytes.size()));
        buf.insert(buf.end(), bytes.begin(), bytes.end());
    }

    // Character data is stored as UCS-2.
    void append_cdata(std::vector<std::uint8_t> &buf, const std::u16string &text) {
        std::vector<std::uint8_t> bytes;

        for (const char16_t c : text) {
            append16(bytes, static_cast<std::uint16_t>(c));
        }

        buf.push_back(SVGB_CDATA);
        append_string(buf, bytes);
    }

    std::vector<std::uint8_t> begin_svgb() {
        std::vector<std::uint8_t> buf;
        append32(buf, SVGB_MAGIC);

        return buf;
    }

    // Ends the attribute list. The byte that follows decides whether the element
    // closes immediately or has content, so the caller writes it next.
    void end_attributes(std::vector<std::uint8_t> &buf) {
        append16(buf, SVGB_ATTR_END);
    }

    std::string convert(const std::vector<std::uint8_t> &svgb, const bool expect_success = true) {
        common::ro_buf_stream in(const_cast<std::uint8_t *>(svgb.data()), svgb.size());

        std::vector<std::uint8_t> out_buf(64 * 1024, 0);
        common::wo_buf_stream out(out_buf.data(), out_buf.size());

        std::vector<loader::svgb_convert_error_description> errors;
        const bool ok = loader::convert_svgb_to_svg(in, out, errors);
        REQUIRE(ok == expect_success);

        return std::string(reinterpret_cast<const char *>(out_buf.data()), static_cast<std::size_t>(out.tell()));
    }
}

TEST_CASE("svgb_text_x_is_a_coordinate_list", "svgb_file") {
    // SVG lets a <text> element place each of its glyphs, so on <text> the x and y
    // attributes are coordinate lists and the binariser writes a count byte in
    // front of the floats. Reading them as a bare float leaves the count byte
    // unconsumed, and every byte after it is interpreted one position out of
    // phase -- which is what dropped the rest of the icon.
    std::vector<std::uint8_t> buf = begin_svgb();

    buf.push_back(ELEM_TEXT);
    append16(buf, ATTR_X);
    buf.push_back(3);
    append_float(buf, 10.0f);
    append_float(buf, 20.0f);
    append_float(buf, 30.0f);
    end_attributes(buf);
    buf.push_back(SVGB_ELEMENT_END);
    buf.push_back(SVGB_FILE_END);

    const std::string svg = convert(buf);

    REQUIRE(svg.find("x=\"10 20 30\"") != std::string::npos);
}

TEST_CASE("svgb_text_y_is_a_coordinate_list_too", "svgb_file") {
    // y takes the same form and is decoded by the same handler; a fix applied to
    // only one of the pair still desynchronises.
    std::vector<std::uint8_t> buf = begin_svgb();

    buf.push_back(ELEM_TEXT);
    append16(buf, ATTR_Y);
    buf.push_back(2);
    append_float(buf, 6.25f);
    append_float(buf, 7.0f);
    end_attributes(buf);
    buf.push_back(SVGB_ELEMENT_END);
    buf.push_back(SVGB_FILE_END);

    const std::string svg = convert(buf);

    REQUIRE(svg.find("y=\"6.25 7\"") != std::string::npos);
}

TEST_CASE("svgb_non_text_x_stays_a_single_float", "svgb_file") {
    // Everywhere other than <text>, x is one number with no count in front of it.
    // Reading a count there would be the same bug in the opposite direction.
    std::vector<std::uint8_t> buf = begin_svgb();

    buf.push_back(ELEM_RECT);
    append16(buf, ATTR_X);
    append_float(buf, 5.0f);
    end_attributes(buf);
    buf.push_back(SVGB_ELEMENT_END);
    buf.push_back(SVGB_FILE_END);

    const std::string svg = convert(buf);

    REQUIRE(svg.find("x=\"5\"") != std::string::npos);
}

TEST_CASE("svgb_text_coordinate_list_can_be_empty", "svgb_file") {
    // A count of zero writes no attribute, but the following element must still
    // decode -- the count byte has been consumed either way.
    std::vector<std::uint8_t> buf = begin_svgb();

    buf.push_back(ELEM_TEXT);
    append16(buf, ATTR_X);
    buf.push_back(0);
    end_attributes(buf);
    buf.push_back(SVGB_ELEMENT_END);
    buf.push_back(ELEM_RECT);
    end_attributes(buf);
    buf.push_back(SVGB_ELEMENT_END);
    buf.push_back(SVGB_FILE_END);

    const std::string svg = convert(buf);

    REQUIRE(svg.find("x=") == std::string::npos);
    REQUIRE(svg.find("<rect") != std::string::npos);
}

TEST_CASE("svgb_character_data_is_consumed_and_emitted", "svgb_file") {
    // The character-data branch used to record "ignored" and return without
    // reading the string, so the text bytes were decoded as elements and
    // attributes. The <rect> after it is the witness: it only survives if the
    // string was consumed.
    std::vector<std::uint8_t> buf = begin_svgb();

    buf.push_back(ELEM_TEXT);
    end_attributes(buf);
    append_cdata(buf, u"Hi");
    buf.push_back(SVGB_ELEMENT_END);
    buf.push_back(ELEM_RECT);
    end_attributes(buf);
    buf.push_back(SVGB_ELEMENT_END);
    buf.push_back(SVGB_FILE_END);

    const std::string svg = convert(buf);

    REQUIRE(svg.find(">Hi</text>") != std::string::npos);
    REQUIRE(svg.find("<rect") != std::string::npos);
}

TEST_CASE("svgb_character_data_is_not_padded_by_indentation", "svgb_file") {
    // Whitespace inside a <text> element is content: indenting the closing tag
    // appends it to the rendered string. It only shows once the element is nested
    // deep enough to carry indentation, which is where real icons put their text.
    std::vector<std::uint8_t> buf = begin_svgb();

    buf.push_back(ELEM_GROUP);
    end_attributes(buf);
    buf.push_back(ELEM_TEXT);
    end_attributes(buf);
    append_cdata(buf, u"Hi");
    buf.push_back(SVGB_ELEMENT_END);
    buf.push_back(SVGB_ELEMENT_END);
    buf.push_back(SVGB_FILE_END);

    const std::string svg = convert(buf);

    REQUIRE(svg.find(">Hi</text>") != std::string::npos);
}

TEST_CASE("svgb_character_data_is_xml_escaped", "svgb_file") {
    // The string is copied into element content, so the three characters that
    // cannot appear there literally have to be escaped or the SVG will not parse.
    std::vector<std::uint8_t> buf = begin_svgb();

    buf.push_back(ELEM_TEXT);
    end_attributes(buf);
    append_cdata(buf, u"a&b<c>d");
    buf.push_back(SVGB_ELEMENT_END);
    buf.push_back(SVGB_FILE_END);

    const std::string svg = convert(buf);

    REQUIRE(svg.find("a&amp;b&lt;c&gt;d") != std::string::npos);
}

TEST_CASE("svgb_string_length_byte_is_unsigned", "svgb_file") {
    // The length byte spans 0..255. Read into a signed char, anything from 128 up
    // is negative, and the read length is nowhere near what the binariser wrote.
    const std::u16string long_text(80, u'x');

    std::vector<std::uint8_t> buf = begin_svgb();

    buf.push_back(ELEM_TEXT);
    end_attributes(buf);
    append_cdata(buf, long_text);
    buf.push_back(SVGB_ELEMENT_END);
    buf.push_back(ELEM_RECT);
    end_attributes(buf);
    buf.push_back(SVGB_ELEMENT_END);
    buf.push_back(SVGB_FILE_END);

    const std::string svg = convert(buf);

    REQUIRE(svg.find(std::string(80, 'x')) != std::string::npos);
    REQUIRE(svg.find("<rect") != std::string::npos);
}

TEST_CASE("svgb_truncated_coordinate_list_fails_cleanly", "svgb_file") {
    // A count that promises more floats than the stream holds must be reported,
    // not read past the end of the buffer.
    std::vector<std::uint8_t> buf = begin_svgb();

    buf.push_back(ELEM_TEXT);
    append16(buf, ATTR_X);
    buf.push_back(4);
    append_float(buf, 1.0f);

    convert(buf, false);
}
