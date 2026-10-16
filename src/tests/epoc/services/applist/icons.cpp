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

    std::optional<app_icon_image> decode(std::vector<std::uint8_t> file, epocver version = epocver::epoc10) {
        common::ro_buf_stream input(file.data(), file.size());
        return decode_mbm_app_icon(nullptr, input, version);
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
