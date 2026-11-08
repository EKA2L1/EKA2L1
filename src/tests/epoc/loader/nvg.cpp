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

    // Builds a direct-command NVG file with one DRAW_PATH command.
    //
    //   0            signature, version, header size, type
    //   26           path data type
    //   36           viewport (x, y, w, h)
    //   header_size  offset vector: count, then one offset pointing at the path data
    //   ...          command block: count, then the DRAW_PATH command
    //   ...          path data: segment count, segment types, padding, coordinates
    //
    // From version 2 the command block starts on a word boundary and carries two
    // bytes of padding after its count, so both are placed the way Symbian's
    // decoder expects to find them.
    std::vector<std::uint8_t> make_nvg(const std::vector<std::uint8_t> &segments,
        const std::vector<std::int32_t> &coords, const std::uint16_t datatype,
        const std::uint8_t version = 1, const std::uint16_t header_size = NVG_HEADER_SIZE) {
        std::vector<std::uint8_t> buf(header_size, 0);

        std::memcpy(buf.data(), "nvg", 3);
        buf[OFF_VERSION] = version;
        put16(buf, OFF_HEADER_SIZE, header_size);
        put16(buf, OFF_PATH_DATATYPE, datatype);

        const float viewport[4] = { 0.0f, 0.0f, 96.0f, 96.0f };
        std::memcpy(buf.data() + OFF_VIEWPORT, viewport, sizeof(viewport));

        // Offset vector: one entry, filled in once the data position is known.
        buf.resize(header_size + 4, 0);
        put16(buf, header_size, 1);

        // An odd offset vector count, so version 2 does not pad the command block.
        std::size_t commands_offset = header_size + 4;

        // Command block: one DRAW_PATH (opcode 7) with the fill bit set, reading
        // entry 0 of the offset vector.
        const std::uint32_t command = (7u << 24) | 0x00020000u;
        buf.resize(commands_offset + ((version >= 2) ? 4 : 2), 0);
        put16(buf, commands_offset, 1);

        for (int i = 0; i < 4; i++) {
            buf.push_back(static_cast<std::uint8_t>((command >> (i * 8)) & 0xFF));
        }

        put16(buf, header_size + 2, static_cast<std::uint16_t>(buf.size()));

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

TEST_CASE("nvg_version_2_command_block_is_found_at_an_odd_header_size", "nvg_file") {
    // Symbian pads the version 2 command block based on the offset vector count
    // alone -- an even count means padded -- not on where the block lands. The
    // two rules only agree while the header size is a multiple of four. With an
    // odd count and a header size of 54, deriving the padding from the address
    // instead skips two bytes too far and the command block is misread.
    const std::string svg = convert(make_nvg({ SEG_MOVE_TO }, { 16, 32 }, PATH_SIXTEEN_BIT, 2, 54));

    REQUIRE(svg.find("M 1 2") != std::string::npos);
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

TEST_CASE("nvg_paint_alpha_scales_the_color_ramp_stop_alpha", "nvg_file") {
    // CNvgEngine::SetColorRampL multiplies each stop's own alpha by the paint
    // alpha. Replacing it instead turned translucent skin gradients opaque,
    // e.g. the progress bar frame that is drawn over the bar's fill.
    std::vector<std::uint8_t> buf(NVG_HEADER_SIZE, 0);
    std::memcpy(buf.data(), "nvg", 3);
    buf[OFF_VERSION] = 1;
    put16(buf, OFF_HEADER_SIZE, NVG_HEADER_SIZE);
    put16(buf, OFF_PATH_DATATYPE, PATH_SIXTEEN_BIT);

    const float viewport[4] = { 0.0f, 0.0f, 96.0f, 96.0f };
    std::memcpy(buf.data() + OFF_VIEWPORT, viewport, sizeof(viewport));

    // Offset vector: paint, colour ramp, path.
    buf.resize(NVG_HEADER_SIZE + 2 + 3 * 2, 0);
    put16(buf, NVG_HEADER_SIZE, 3);

    const std::uint32_t commands[3] = {
        (4u << 24) | (0x80u << 16) | 0u, // SET_FILL_PAINT, paint alpha 0x80
        (6u << 24) | 1u, // SET_COLOR_RAMP
        (7u << 24) | 0x00020000u | 2u // DRAW_PATH, fill
    };

    append16(buf, 3);
    for (const std::uint32_t command : commands) {
        append32(buf, static_cast<std::int32_t>(command));
    }

    auto append_float = [&buf](const float value) {
        std::int32_t raw = 0;
        std::memcpy(&raw, &value, sizeof(raw));
        append32(buf, raw);
    };

    // Linear gradient paint: type, then x0, y0, x1, y1.
    put16(buf, NVG_HEADER_SIZE + 2, static_cast<std::uint16_t>(buf.size()));
    append32(buf, 2);
    for (const float value : { 0.0f, 0.0f, 0.0f, 96.0f }) {
        append_float(value);
    }

    // Two stops (offset, r, g, b, a) with alpha 1.0 and 0.5.
    put16(buf, NVG_HEADER_SIZE + 4, static_cast<std::uint16_t>(buf.size()));
    append32(buf, 2 << 16);
    for (const float value : { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.5f }) {
        append_float(value);
    }

    put16(buf, NVG_HEADER_SIZE + 6, static_cast<std::uint16_t>(buf.size()));
    append16(buf, 1);
    buf.push_back(SEG_MOVE_TO);
    buf.push_back(0);
    append16(buf, 16);
    append16(buf, 32);

    const std::string svg = convert(buf);

    REQUIRE(svg.find("stop-opacity=\"0.50196") != std::string::npos);
    REQUIRE(svg.find("stop-opacity=\"0.25098") != std::string::npos);
}

// NVG-TLV: a recording of OpenVG calls, replayed as svgt's nvgdecoder CTLVRenderer does.
namespace {
    constexpr std::size_t OFF_NVG_TYPE = 6;
    constexpr std::uint16_t NVG_TYPE_TLV = 1;

    enum : std::uint8_t {
        TLV_CLEAR = 0,
        TLV_SETI = 1,
        TLV_SETF = 2,
        TLV_SET_COLOR = 7,
        TLV_SET_PAINT = 8,
        TLV_LOAD_MATRIX = 9,
        TLV_APPEND_PATH_DATA = 15,
        TLV_DRAW_PATH = 16,
        TLV_CREATE_PAINT = 22,
        TLV_CREATE_PATH = 23
    };

    constexpr std::uint16_t VG_MATRIX_MODE = 0x1100;
    constexpr std::uint16_t VG_FILL_RULE = 0x1101;
    constexpr std::uint16_t VG_STROKE_LINE_WIDTH = 0x1110;
    constexpr std::uint16_t VG_NON_ZERO = 0x1901;
    constexpr std::uint16_t VG_MATRIX_PATH_USER_TO_SURFACE = 0x1400;
    constexpr std::uint8_t VG_PATH_DATATYPE_S_32 = 2;
    constexpr std::uint8_t VG_PATH_DATATYPE_F = 3;
    constexpr std::uint8_t VG_STROKE_PATH = 1;
    constexpr std::uint8_t VG_FILL_PATH = 2;
    constexpr std::uint8_t SEG_LINE_TO = 2 << 1;
    constexpr std::uint8_t SEG_SCCWARC_TO = 9 << 1;

    struct tlv_builder {
        std::vector<std::uint8_t> buf;

        tlv_builder() : buf(NVG_HEADER_SIZE, 0) {
            std::memcpy(buf.data(), "nvg", 3);
            buf[OFF_VERSION] = 1;
            put16(buf, OFF_HEADER_SIZE, NVG_HEADER_SIZE);
            put16(buf, OFF_NVG_TYPE, NVG_TYPE_TLV);
        }

        tlv_builder &op(const std::uint8_t opcode) {
            buf.push_back(opcode);
            return *this;
        }

        tlv_builder &u8(const std::uint8_t value) {
            buf.push_back(value);
            return *this;
        }

        tlv_builder &u16(const std::uint16_t value) {
            append16(buf, static_cast<std::int16_t>(value));
            return *this;
        }

        tlv_builder &i32(const std::int32_t value) {
            append32(buf, value);
            return *this;
        }

        tlv_builder &f32(const float value) {
            std::int32_t raw = 0;
            std::memcpy(&raw, &value, sizeof(raw));
            return i32(raw);
        }

        tlv_builder &clear(const float width, const float height) {
            return op(TLV_CLEAR).f32(0).f32(0).f32(width).f32(height);
        }

        tlv_builder &create_path(const std::uint8_t datatype, const float scale, const float bias) {
            return op(TLV_CREATE_PATH).i32(0).u8(datatype).f32(scale).f32(bias).i32(0).i32(0).i32(0).i32(1);
        }

        tlv_builder &append_path(const std::vector<std::uint8_t> &segments, const std::vector<std::int32_t> &coords) {
            op(TLV_APPEND_PATH_DATA).i32(1).u16(static_cast<std::uint16_t>(segments.size()));
            buf.insert(buf.end(), segments.begin(), segments.end());
            u16(static_cast<std::uint16_t>(coords.size()));
            for (const std::int32_t coord : coords) {
                i32(coord);
            }
            return *this;
        }

        tlv_builder &append_float_path(const std::vector<std::uint8_t> &segments, const std::vector<float> &coords) {
            std::vector<std::int32_t> raw(coords.size());
            std::memcpy(raw.data(), coords.data(), coords.size() * sizeof(float));
            return append_path(segments, raw);
        }

        tlv_builder &draw(const std::uint16_t modes) {
            return op(TLV_DRAW_PATH).i32(1).u16(modes);
        }
    };

    std::string convert_tlv(std::vector<std::uint8_t> nvg, const int width = -1, const int height = -1) {
        common::ro_buf_stream in(nvg.data(), nvg.size());
        common::wo_growable_buf_stream out;

        loader::nvg_options options;
        options.width = width;
        options.height = height;

        std::vector<loader::nvg_convert_error_description> errors;
        REQUIRE(loader::convert_nvg_to_svg(in, out, errors, &options));

        return out.content();
    }
}

TEST_CASE("nvg_tlv_first_clear_stretches_the_canvas_to_the_target", "nvg_file") {
    // CTLVRenderer::DvgClearL maps the recorded canvas onto the target size
    // without keeping the aspect ratio, and the output flips the y-up surface.
    tlv_builder tlv;
    tlv.clear(100, 50).create_path(VG_PATH_DATATYPE_F, 1.0f, 0.0f).append_float_path({ SEG_MOVE_TO, SEG_LINE_TO }, { 1, 2, 3, 4 }).draw(VG_FILL_PATH);

    const std::string svg = convert_tlv(tlv.buf, 200, 200);

    REQUIRE(svg.find("viewBox=\"0 0 200 200\"") != std::string::npos);
    REQUIRE(svg.find("matrix(1 0 0 -1 0 200)") != std::string::npos);
    REQUIRE(svg.find("transform=\"matrix(2 0 0 4 0 0)\"") != std::string::npos);
    REQUIRE(svg.find("M 1 2 L 3 4") != std::string::npos);
}

TEST_CASE("nvg_tlv_canvas_defaults_to_the_recorded_clear", "nvg_file") {
    tlv_builder tlv;
    tlv.clear(240, 156).create_path(VG_PATH_DATATYPE_F, 1.0f, 0.0f).append_float_path({ SEG_MOVE_TO }, { 1, 2 }).draw(VG_FILL_PATH);

    const std::string svg = convert_tlv(tlv.buf);

    REQUIRE(svg.find("viewBox=\"0 0 240 156\"") != std::string::npos);
    REQUIRE(svg.find("transform=\"matrix(1 0 0 1 0 0)\"") != std::string::npos);
}

TEST_CASE("nvg_tlv_integer_coordinates_use_the_path_scale_and_bias", "nvg_file") {
    // Coordinates are always recorded as 32 bits; S_32 ones are scaled like vgCreatePath says.
    tlv_builder tlv;
    tlv.clear(10, 10).create_path(VG_PATH_DATATYPE_S_32, 0.5f, 1.0f).append_path({ SEG_MOVE_TO | SEG_RELATIVE }, { -4, 6 }).draw(VG_FILL_PATH);

    REQUIRE(convert_tlv(tlv.buf).find("m -1 4") != std::string::npos);
}

TEST_CASE("nvg_tlv_path_matrix_is_relative_to_the_fitted_canvas", "nvg_file") {
    // DvgLoadMatrixL premultiplies path and image matrices by the fit-to-target matrix.
    tlv_builder tlv;
    tlv.clear(10, 10).op(TLV_SETI).u16(VG_MATRIX_MODE).u16(VG_MATRIX_PATH_USER_TO_SURFACE);
    tlv.op(TLV_LOAD_MATRIX).f32(1).f32(0).f32(0).f32(0).f32(-1).f32(0).f32(3).f32(5).f32(1);
    tlv.create_path(VG_PATH_DATATYPE_F, 1.0f, 0.0f).append_float_path({ SEG_MOVE_TO }, { 0, 0 }).draw(VG_FILL_PATH);

    REQUIRE(convert_tlv(tlv.buf, 20, 20).find("transform=\"matrix(2 0 0 -2 6 10)\"") != std::string::npos);
}

TEST_CASE("nvg_tlv_paint_color_and_fill_rule", "nvg_file") {
    tlv_builder tlv;
    tlv.clear(10, 10).op(TLV_CREATE_PAINT).i32(7).op(TLV_SET_COLOR).i32(7).i32(static_cast<std::int32_t>(0xFF800040u));
    tlv.op(TLV_SET_PAINT).i32(7).u8(VG_FILL_PATH);
    tlv.create_path(VG_PATH_DATATYPE_F, 1.0f, 0.0f).append_float_path({ SEG_MOVE_TO }, { 0, 0 }).draw(VG_FILL_PATH);
    tlv.op(TLV_SETI).u16(VG_FILL_RULE).u16(VG_NON_ZERO);
    tlv.append_float_path({ SEG_MOVE_TO }, { 1, 1 }).draw(VG_FILL_PATH);

    const std::string svg = convert_tlv(tlv.buf);

    REQUIRE(svg.find("fill=\"#FF8000\" fill-opacity=\"0.2509804\" fill-rule=\"evenodd\"") != std::string::npos);
    REQUIRE(svg.find("fill-rule=\"nonzero\"") != std::string::npos);
}

TEST_CASE("nvg_tlv_draw_consumes_the_path", "nvg_file") {
    // The renderer clears its single path after every vgDrawPath.
    tlv_builder tlv;
    tlv.clear(10, 10).create_path(VG_PATH_DATATYPE_F, 1.0f, 0.0f);
    tlv.append_float_path({ SEG_MOVE_TO }, { 1, 1 }).draw(VG_FILL_PATH);
    tlv.append_float_path({ SEG_MOVE_TO }, { 2, 2 }).draw(VG_STROKE_PATH);

    const std::string svg = convert_tlv(tlv.buf);

    REQUIRE(svg.find("d=\"M 1 1 \"") != std::string::npos);
    REQUIRE(svg.find("d=\"M 2 2 \"") != std::string::npos);
    REQUIRE(svg.find("stroke=\"#000000\"") != std::string::npos);
}

TEST_CASE("nvg_tlv_counter_clockwise_arc_sweeps_positive", "nvg_file") {
    tlv_builder tlv;
    tlv.clear(10, 10).create_path(VG_PATH_DATATYPE_F, 1.0f, 0.0f);
    tlv.append_float_path({ SEG_MOVE_TO, SEG_SCCWARC_TO }, { 0, 0, 2, 3, 0, 4, 0 }).draw(VG_FILL_PATH);

    REQUIRE(convert_tlv(tlv.buf).find("A 2 3 0 0 1 4 0") != std::string::npos);
}

TEST_CASE("nvg_tlv_stroke_width_is_emitted", "nvg_file") {
    tlv_builder tlv;
    tlv.clear(10, 10).op(TLV_SETF).u16(VG_STROKE_LINE_WIDTH).f32(2.5f);
    tlv.create_path(VG_PATH_DATATYPE_F, 1.0f, 0.0f).append_float_path({ SEG_MOVE_TO, SEG_LINE_TO }, { 0, 0, 1, 1 }).draw(VG_STROKE_PATH);

    const std::string svg = convert_tlv(tlv.buf);

    REQUIRE(svg.find("fill=\"none\"") != std::string::npos);
    REQUIRE(svg.find("stroke-width=\"2.5\"") != std::string::npos);
}

TEST_CASE("nvg_tlv_trailing_padding_keeps_what_was_drawn", "nvg_file") {
    // Extended bitmaps hand over their whole allocation, so the stream ends in
    // zero padding that decodes as clears and finally a truncated command.
    tlv_builder tlv;
    tlv.clear(10, 10).create_path(VG_PATH_DATATYPE_F, 1.0f, 0.0f).append_float_path({ SEG_MOVE_TO }, { 1, 1 }).draw(VG_FILL_PATH);
    tlv.buf.resize(tlv.buf.size() + 40, 0);

    const std::string svg = convert_tlv(tlv.buf);

    REQUIRE(svg.find("M 1 1") != std::string::npos);
    REQUIRE(svg.find("</svg>") != std::string::npos);
}
