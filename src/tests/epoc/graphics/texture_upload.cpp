// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch.hpp>
#include <drivers/graphics/texture_upload.h>
#include <limits>

using eka2l1::drivers::bgra8888_to_rgba;

TEST_CASE("BGRA8888 uploads preserve channels and alpha across padded rows", "[graphics][texture]") {
    const std::vector<std::uint8_t> source{ 0, 0, 255, 128, 255, 0, 0, 255, 99, 99, 99, 99, 0, 255, 0, 64, 7, 8, 9, 0 };
    std::vector<std::uint8_t> rgba;
    REQUIRE(bgra8888_to_rgba(rgba, source.data(), source.size(), 2, 2, 3, 4));
    CHECK(rgba == std::vector<std::uint8_t>{ 255, 0, 0, 128, 0, 0, 255, 255, 0, 255, 0, 64, 9, 8, 7, 0 });
    CHECK_FALSE(bgra8888_to_rgba(rgba, source.data(), source.size() - 1, 2, 2, 3, 4));

    const std::vector<std::uint8_t> aligned{ 3, 2, 1, 4, 99, 99, 99, 99, 7, 6, 5, 8 };
    REQUIRE(bgra8888_to_rgba(rgba, aligned.data(), aligned.size(), 1, 2, 0, 8));
    CHECK(rgba == std::vector<std::uint8_t>{ 1, 2, 3, 4, 5, 6, 7, 8 });

    const std::vector<std::uint8_t> overlapping{ 3, 2, 1, 4, 7, 6, 5, 8, 11, 10, 9, 12 };
    REQUIRE(bgra8888_to_rgba(rgba, overlapping.data(), overlapping.size(), 2, 2, 1, 4));
    CHECK(rgba == std::vector<std::uint8_t>{ 1, 2, 3, 4, 5, 6, 7, 8, 5, 6, 7, 8, 9, 10, 11, 12 });
}

TEST_CASE("BGRA8888 uploads reject invalid layouts before reading or allocating", "[graphics][texture]") {
    const std::uint8_t source[4] = {};
    std::vector<std::uint8_t> rgba{ 42 };
    const auto max = std::numeric_limits<std::size_t>::max();
    CHECK_FALSE(bgra8888_to_rgba(rgba, nullptr, 4, 1, 1, 0, 4));
    CHECK_FALSE(bgra8888_to_rgba(rgba, source, 4, 0, 1, 0, 4));
    CHECK_FALSE(bgra8888_to_rgba(rgba, source, 4, 1, 0, 0, 4));
    CHECK_FALSE(bgra8888_to_rgba(rgba, source, 4, 1, 1, 0, 3));
    CHECK_FALSE(bgra8888_to_rgba(rgba, source, 4, 2, 2, 1, 4));
    CHECK_FALSE(bgra8888_to_rgba(rgba, source, max, max, 1, 0, 4));
    CHECK_FALSE(bgra8888_to_rgba(rgba, source, max, 1, 1, max / 4, 8));
    CHECK_FALSE(bgra8888_to_rgba(rgba, source, max, 1, max / 4 + 1, 0, 4));
    CHECK_FALSE(bgra8888_to_rgba(rgba, source, max, 1, max / 8 + 2, 2, 4));
    CHECK(rgba == std::vector<std::uint8_t>{ 42 });
}

TEST_CASE("RGB444 bitmap uploads preserve all 4096 colours with opaque alpha", "[graphics][texture]") {
    std::vector<std::uint8_t> source;
    for (std::uint16_t colour = 0; colour < 4096; ++colour) {
        source.push_back(static_cast<std::uint8_t>(colour));
        source.push_back(static_cast<std::uint8_t>(colour >> 8));
    }
    std::vector<std::uint16_t> rgba;
    REQUIRE(eka2l1::drivers::rgb444_to_rgba4444(rgba, source.data(), source.size(), 4096, 1, 0));
    REQUIRE(rgba.size() == 4096);
    for (std::size_t red = 0; red < 16; ++red) {
        for (std::size_t green = 0; green < 16; ++green) {
            for (std::size_t blue = 0; blue < 16; ++blue) {
                const auto pixel = rgba[red * 256 + green * 16 + blue];
                CHECK(((pixel >> 12) & 0xF) * 17 == red * 17);
                CHECK(((pixel >> 8) & 0xF) * 17 == green * 17);
                CHECK(((pixel >> 4) & 0xF) * 17 == blue * 17);
                CHECK((pixel & 0xF) == 0xF);
            }
        }
    }
}

TEST_CASE("RGB444 bitmap uploads handle unaligned input and padded odd-width rows", "[graphics][texture]") {
    const std::vector<std::uint8_t> source{
        0xEE,
        0x23, 0x01, 0x56, 0x04, 0x89, 0x07, 0xAA, 0xAA, 0xBB, 0xBB, 0xCC, 0xCC,
        0xBC, 0x0A, 0xEF, 0x0D, 0x46, 0xF2
    };
    const std::vector<std::uint16_t> expected{ 0x123F, 0x456F, 0x789F, 0, 0xABCF, 0xDEFF, 0x246F, 0 };
    std::vector<std::uint16_t> rgba;
    REQUIRE(eka2l1::drivers::rgb444_to_rgba4444(rgba, source.data() + 1, source.size() - 1, 3, 2, 5));
    CHECK(rgba == expected);
    CHECK_FALSE(eka2l1::drivers::rgb444_to_rgba4444(rgba, source.data() + 1, source.size() - 2, 3, 2, 5));
    CHECK(rgba == expected);
    REQUIRE(eka2l1::drivers::rgb444_to_rgba4444(rgba, source.data() + 1, 0, 3, 2, 5));
    CHECK(rgba == expected);

    const std::uint8_t aligned[]{ 0x23, 0x01, 0xFF, 0xFF, 0x56, 0x04 };
    REQUIRE(eka2l1::drivers::rgb444_to_rgba4444(rgba, aligned, sizeof(aligned), 1, 2, 0));
    CHECK(rgba == std::vector<std::uint16_t>{ 0x123F, 0, 0x456F, 0 });
}

TEST_CASE("RGB444 bitmap uploads reject invalid layouts without changing the destination", "[graphics][texture]") {
    const std::uint8_t source[2] = {};
    std::vector<std::uint16_t> rgba{ 0x123F };
    const auto max = std::numeric_limits<std::size_t>::max();
    CHECK_FALSE(eka2l1::drivers::rgb444_to_rgba4444(rgba, nullptr, 2, 1, 1, 0));
    CHECK_FALSE(eka2l1::drivers::rgb444_to_rgba4444(rgba, source, 2, 0, 1, 0));
    CHECK_FALSE(eka2l1::drivers::rgb444_to_rgba4444(rgba, source, 2, 1, 0, 0));
    CHECK_FALSE(eka2l1::drivers::rgb444_to_rgba4444(rgba, source, max, max, 1, 0));
    CHECK_FALSE(eka2l1::drivers::rgb444_to_rgba4444(rgba, source, max, 1, 1, max));
    CHECK_FALSE(eka2l1::drivers::rgb444_to_rgba4444(rgba, source, max, 1, max / 2 + 1, 0));
    CHECK_FALSE(eka2l1::drivers::rgb444_to_rgba4444(rgba, source, max, 1, max / 4 + 2, 0));
    CHECK_FALSE(eka2l1::drivers::rgb444_to_rgba4444(rgba, source, max, 1, max / 4 + 1, 0));
    CHECK_FALSE(eka2l1::drivers::rgb444_to_rgba4444(rgba, source, max, max / 2, 1, 1));
    CHECK(rgba == std::vector<std::uint16_t>{ 0x123F });
}
