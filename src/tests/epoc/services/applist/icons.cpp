// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch.hpp>
#include <common/buffer.h>
#include <services/applist/icons.h>
#include <array>

using namespace eka2l1;

namespace {
    void append_word(std::vector<std::uint8_t> &data, std::uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) data.push_back(value >> shift);
    }

    std::vector<std::uint8_t> make_mbm(const std::vector<std::uint8_t> &mask, std::uint32_t bpp,
        std::uint32_t colour, std::uint32_t compression = 0, std::uint32_t mask_width = 3) {
        std::vector<std::uint8_t> image;
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 3; ++x) image.insert(image.end(), { 64, 128, 255 });
            image.insert(image.end(), { 0, 0, 0 });
        }
        const std::uint32_t trailer = 20 + 40 + image.size() + (mask.empty() ? 0 : 40 + mask.size());
        std::vector<std::uint8_t> file;
        for (auto value : { 0x10000037u, 0x10000042u, 0u, 0u, trailer }) append_word(file, value);
        auto append_bitmap = [&](const auto &pixels, std::uint32_t width, std::uint32_t depth,
                                 std::uint32_t color, std::uint32_t encoding) {
            for (auto value : { static_cast<std::uint32_t>(40 + pixels.size()), 40u, width, 2u,
                     0u, 0u, depth, color, 0u, encoding }) append_word(file, value);
            file.insert(file.end(), pixels.begin(), pixels.end());
        };
        append_bitmap(image, 3, 24, 1, 0);
        if (!mask.empty()) append_bitmap(mask, mask_width, bpp, colour, compression);
        append_word(file, mask.empty() ? 1 : 2);
        append_word(file, 20);
        if (!mask.empty()) append_word(file, 20 + 40 + image.size());
        return file;
    }

    std::optional<app_icon_image> decode(std::vector<std::uint8_t> file, epocver version = epocver::epoc10,
        bool uiq = false, int icon_count = 1) {
        common::ro_buf_stream input(file.data(), file.size());
        return decode_mbm_app_icon(nullptr, input, version, uiq, icon_count);
    }

    // Square 24-bit bitmaps paired with opaque Gray256 masks of the given sides.
    std::vector<std::uint8_t> make_icon_pairs(const std::vector<std::pair<std::uint32_t, std::uint32_t>> &sides) {
        std::vector<std::uint8_t> file(20);
        std::vector<std::uint32_t> offsets;
        auto append_bitmap = [&](std::uint32_t side, std::uint32_t depth, std::uint32_t color, std::uint8_t value) {
            // CFbsBitmap::ScanLineLength: 24-bit rows align to 12 bytes, 8-bit rows to a word.
            const std::uint32_t stride = depth == 24 ? (side * 3 + 11) / 12 * 12 : (side + 3) & ~3u;
            offsets.push_back(static_cast<std::uint32_t>(file.size()));
            for (auto word : { 40 + stride * side, 40u, side, side, 0u, 0u, depth, color, 0u, 0u }) append_word(file, word);
            file.insert(file.end(), stride * side, value);
        };
        for (const auto &[bitmap_side, mask_side] : sides) {
            append_bitmap(bitmap_side, 24, 1, 0x40);
            append_bitmap(mask_side, 8, 0, 0xFF);
        }
        const std::uint32_t trailer = static_cast<std::uint32_t>(file.size());
        append_word(file, static_cast<std::uint32_t>(offsets.size()));
        for (auto offset : offsets) append_word(file, offset);
        std::vector<std::uint8_t> header;
        for (auto value : { 0x10000037u, 0x10000042u, 0u, 0u, trailer }) append_word(header, value);
        std::copy(header.begin(), header.end(), file.begin());
        return file;
    }

    void check_pixels(const app_icon_image &icon, const std::array<std::uint8_t, 6> &alpha) {
        REQUIRE(icon.size == vec2(3, 2));
        REQUIRE(icon.pixels.size() == 24);
        for (std::size_t i = 0; i < alpha.size(); ++i) {
            REQUIRE(icon.pixels[i * 4] == alpha[i]);
            REQUIRE(icon.pixels[i * 4 + 1] == (128u * alpha[i] + 127) / 255);
            REQUIRE(icon.pixels[i * 4 + 2] == (64u * alpha[i] + 127) / 255);
            REQUIRE(icon.pixels[i * 4 + 3] == alpha[i]);
        }
    }
}

TEST_CASE("Host MBM icons decode compressed monochrome masks into owned premultiplied pixels", "app_icon") {
    const auto icon = decode(make_mbm({ 0xF8, 0x02, 0, 0, 0, 0x05, 0, 0, 0 }, 1, 0, 1));
    REQUIRE(icon);
    check_pixels(*icon, { 255, 0, 255, 0, 255, 0 });
}

TEST_CASE("Host MBM icons preserve soft mask opacity", "app_icon") {
    const auto icon = decode(make_mbm({ 0, 64, 128, 0, 255, 32, 16, 0 }, 8, 0));
    REQUIRE(icon);
    check_pixels(*icon, { 0, 64, 128, 255, 32, 16 });
}

TEST_CASE("Host MBM icons select the ROM generation's colour mask contract", "app_icon") {
    const auto file = make_mbm({ 0, 0, 0, 64, 64, 64, 255, 255, 255, 0, 0, 0,
        0, 0, 255, 0, 255, 0, 255, 0, 0, 0, 0, 0 }, 24, 1);
    const auto modern = decode(file);
    const auto classic = decode(file, epocver::epoc94);
    REQUIRE(modern);
    REQUIRE(classic);
    check_pixels(*modern, { 0, 64, 255, 63, 159, 31 });
    check_pixels(*classic, { 255, 255, 0, 255, 255, 255 });
}

TEST_CASE("Host MBM icons tolerate absent or mismatched masks", "app_icon") {
    const auto absent = decode(make_mbm({}, 0, 0));
    const auto mismatched = decode(make_mbm({ 255, 255, 255, 255, 255, 255, 255, 255 }, 8, 0, 0, 2));
    REQUIRE(absent);
    REQUIRE(mismatched);
    check_pixels(*absent, { 255, 255, 255, 255, 255, 255 });
    check_pixels(*mismatched, { 255, 255, 255, 255, 255, 255 });
}

TEST_CASE("Host MBM icon decoding rejects incomplete headers", "app_icon") {
    REQUIRE_FALSE(decode({ 0x37, 0, 0, 0x10 }));
}

TEST_CASE("Host MBM icons follow UIQ's non-inverted mask contract", "app_icon") {
    const auto colour = decode(make_mbm({ 0, 0, 0, 64, 64, 64, 255, 255, 255, 0, 0, 0,
        0, 0, 255, 0, 255, 0, 255, 0, 0, 0, 0, 0 }, 24, 1), epocver::epoc94, true);
    const auto mono = decode(make_mbm({ 0xF8, 0x02, 0, 0, 0, 0x05, 0, 0, 0 }, 1, 0, 1), epocver::epoc94, true);
    REQUIRE(colour);
    REQUIRE(mono);
    check_pixels(*colour, { 0, 64, 255, 63, 159, 31 });
    check_pixels(*mono, { 0, 255, 0, 255, 0, 255 });
}

TEST_CASE("Host MBM icons show the largest registered pair with a matching mask", "app_icon") {
    const auto file = make_icon_pairs({ { 18, 18 }, { 40, 44 }, { 32, 32 }, { 64, 64 } });
    const auto registered = decode(file, epocver::epoc94, true, 3);
    const auto first_only = decode(file, epocver::epoc94, true, 0);
    REQUIRE(registered);
    REQUIRE(first_only);
    REQUIRE(registered->size == vec2(32, 32));
    REQUIRE(first_only->size == vec2(18, 18));
    REQUIRE(registered->pixels[3] == 255);
}
