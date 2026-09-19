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

#include <common/buffer.h>
#include <services/fbs/bitmap.h>

#include <catch2/catch.hpp>

#include <cstdint>
#include <vector>

using namespace eka2l1;

TEST_CASE("gray256_decode_distinguishes_colour_from_mask_opacity", "icon_mask") {
    // TRgb::Gray256() uses the opaque RGB constructor; BITGDI alone treats
    // EGray256 mask samples as opacity (RGB.CPP, GDI.INL, BITBLT.CPP).
    const bool as_mask = GENERATE(false, true);
    std::uint8_t samples[] = {0, 17, 128, 250, 255, 0, 0, 0,
        255, 250, 128, 17, 0, 0, 0, 0};
    loader::sbm_header header{};
    header.size_pixels = eka2l1::vec2(5, 2);
    header.bit_per_pixels = epoc::get_bpp_from_display_mode(epoc::display_mode::gray256);
    header.color = epoc::get_bitmap_color_from_display_mode(epoc::display_mode::gray256);
    common::ro_buf_stream source(samples, sizeof(samples));
    std::vector<std::uint8_t> rgba(5 * 2 * 4);
    common::wo_buf_stream destination(rgba.data(), rgba.size());

    REQUIRE(epoc::convert_to_rgba8888(nullptr, source, destination, header, -1,
        epoc::bitmap_file_no_compression, as_mask));

    for (std::size_t y = 0; y < 2; y++) {
        for (std::size_t x = 0; x < 5; x++) {
            const std::size_t offset = (y * 5 + x) * 4;
            const std::uint8_t level = samples[y * 8 + x];
            REQUIRE(rgba[offset] == level);
            REQUIRE(rgba[offset + 1] == level);
            REQUIRE(rgba[offset + 2] == level);
            REQUIRE(rgba[offset + 3] == (as_mask ? level : 255));
        }
    }

    if (as_mask) {
        std::vector<std::uint8_t> icon(rgba.size(), 255);
        epoc::apply_icon_mask_alpha(icon.data(), rgba.data(), 5, 2, epoc::display_mode::gray256);
        for (std::size_t y = 0; y < 2; y++) {
            for (std::size_t x = 0; x < 5; x++) {
                REQUIRE(icon[(y * 5 + x) * 4 + 3] == samples[y * 8 + x]);
            }
        }
    }
}

namespace {
    constexpr std::size_t ICON_SIDE = 4;

    // Opaque white icon pixels; only the alpha channel is interesting here.
    std::vector<std::uint8_t> make_icon() {
        return std::vector<std::uint8_t>(ICON_SIDE * ICON_SIDE * 4, 0xFF);
    }

    // A mask converted to RGBA: the grey level lands in every colour channel, and
    // make_standard_mask has already turned "pure white" into alpha 0xFF.
    std::vector<std::uint8_t> make_mask(const std::vector<std::uint8_t> &levels) {
        std::vector<std::uint8_t> mask(ICON_SIDE * ICON_SIDE * 4, 0);

        for (std::size_t i = 0; i < levels.size(); i++) {
            const std::uint8_t level = levels[i];

            mask[i * 4 + 0] = level;
            mask[i * 4 + 1] = level;
            mask[i * 4 + 2] = level;
            mask[i * 4 + 3] = (level >= 250) ? 0xFF : 0x00;
        }

        return mask;
    }

    std::uint8_t alpha_at(const std::vector<std::uint8_t> &rgba, const std::size_t index) {
        return rgba[index * 4 + 3];
    }
}

TEST_CASE("icon_mask_gray256_is_alpha", "icon_mask") {
    // BITGDI alpha blends only when the mask is EGray256, and then aInvertMask is
    // ignored: the mask's luminance is the icon's alpha, white opaque.
    std::vector<std::uint8_t> levels(ICON_SIDE * ICON_SIDE, 128);
    levels[0] = 0;
    levels[1] = 255;

    std::vector<std::uint8_t> icon = make_icon();
    const std::vector<std::uint8_t> mask = make_mask(levels);

    epoc::apply_icon_mask_alpha(icon.data(), mask.data(), ICON_SIDE, ICON_SIDE,
        epoc::display_mode::gray256);

    REQUIRE(alpha_at(icon, 0) == 0);
    REQUIRE(alpha_at(icon, 1) == 255);
    REQUIRE(alpha_at(icon, 2) == 128);
}

TEST_CASE("icon_mask_colour_key_inverts_the_white_backdrop", "icon_mask") {
    // Any other display mode is a stencil. make_standard_mask has put "this pixel
    // is pure white" in the alpha channel, and Avkon's aInvertMask=ETrue makes
    // white keep the destination, so transparency is the inverse of that test.
    std::vector<std::uint8_t> levels(ICON_SIDE * ICON_SIDE, 0);
    levels[0] = 255;

    std::vector<std::uint8_t> icon = make_icon();
    const std::vector<std::uint8_t> mask = make_mask(levels);

    epoc::apply_icon_mask_alpha(icon.data(), mask.data(), ICON_SIDE, ICON_SIDE,
        epoc::display_mode::color64k);

    REQUIRE(alpha_at(icon, 0) == 0);
    REQUIRE(alpha_at(icon, 1) == 255);
}

TEST_CASE("icon_mask_mono_stencil_keeps_its_white_backdrop_transparent", "icon_mask") {
    // The common legacy AIF case: an EGray2 mask framing the artwork in white.
    std::vector<std::uint8_t> levels(ICON_SIDE * ICON_SIDE, 255);
    levels[ICON_SIDE + 1] = 0;

    std::vector<std::uint8_t> icon = make_icon();
    const std::vector<std::uint8_t> mask = make_mask(levels);

    epoc::apply_icon_mask_alpha(icon.data(), mask.data(), ICON_SIDE, ICON_SIDE,
        epoc::display_mode::gray2);

    REQUIRE(alpha_at(icon, 0) == 0);
    REQUIRE(alpha_at(icon, ICON_SIDE + 1) == 255);
}

TEST_CASE("icon_mask_colour256_is_a_stencil_not_an_alpha_ramp", "icon_mask") {
    // EColor256 is 8bpp like EGray256 but is not an alpha mask. The N-Gage ROM's
    // fmradio icon stores its stencil this way - white backdrop, artwork at
    // palette index 0xE1, which is 0x111111. Read as alpha that artwork would be
    // 93% transparent while the backdrop stayed opaque: an inside-out icon.
    std::vector<std::uint8_t> levels(ICON_SIDE * ICON_SIDE, 255);
    levels[ICON_SIDE + 1] = 0x11;
    levels[ICON_SIDE + 2] = 0x00;

    std::vector<std::uint8_t> icon = make_icon();
    const std::vector<std::uint8_t> mask = make_mask(levels);

    epoc::apply_icon_mask_alpha(icon.data(), mask.data(), ICON_SIDE, ICON_SIDE,
        epoc::display_mode::color256);

    REQUIRE(alpha_at(icon, 0) == 0);
    REQUIRE(alpha_at(icon, ICON_SIDE + 1) == 255);
    REQUIRE(alpha_at(icon, ICON_SIDE + 2) == 255);
}

TEST_CASE("icon_mask_full_bleed_gray256_mask_stays_opaque", "icon_mask") {
    // An entirely white EGray256 mask is a full-bleed icon: every pixel opaque.
    const std::vector<std::uint8_t> levels(ICON_SIDE * ICON_SIDE, 255);

    std::vector<std::uint8_t> icon = make_icon();
    const std::vector<std::uint8_t> mask = make_mask(levels);

    epoc::apply_icon_mask_alpha(icon.data(), mask.data(), ICON_SIDE, ICON_SIDE,
        epoc::display_mode::gray256);

    for (std::size_t i = 0; i < ICON_SIDE * ICON_SIDE; i++) {
        REQUIRE(alpha_at(icon, i) == 255);
    }
}

TEST_CASE("icon_mask_ignores_missing_buffers", "icon_mask") {
    std::vector<std::uint8_t> icon = make_icon();
    const std::vector<std::uint8_t> mask = make_mask(std::vector<std::uint8_t>(ICON_SIDE * ICON_SIDE, 128));

    epoc::apply_icon_mask_alpha(nullptr, mask.data(), ICON_SIDE, ICON_SIDE, epoc::display_mode::gray256);
    epoc::apply_icon_mask_alpha(icon.data(), nullptr, ICON_SIDE, ICON_SIDE, epoc::display_mode::gray256);

    REQUIRE(alpha_at(icon, 0) == 255);
}

TEST_CASE("bitwise_bitmap_reports_the_mode_it_was_built_with", "icon_mask") {
    epoc::bitwise_bitmap bmp{};
    bmp.settings_.initial_display_mode(epoc::display_mode::gray256);
    bmp.settings_.current_display_mode(epoc::display_mode::gray256);

    REQUIRE(bmp.current_display_mode() == epoc::display_mode::gray256);

    // EKA1 ROM bitmaps carry the initial mode only; the current byte reads ENone.
    epoc::bitwise_bitmap rom{};
    rom.settings_.initial_display_mode(epoc::display_mode::color256);

    REQUIRE(rom.settings_.current_display_mode() == epoc::display_mode::none);
    REQUIRE(rom.current_display_mode() == epoc::display_mode::color256);
}

TEST_CASE("bitmap_header_colour_field_round_trips_the_display_mode", "bitmap_header") {
    // get_display_mode_from_bpp() and get_bitmap_color_from_display_mode() are how a
    // mode survives the header, so they have to be inverses. bmconv's own table
    // (PBMCOMP.CPP) is what both follow: 8bpp splits into EGray256 / EColor256 on the
    // colour field alone, 4bpp into EGray16 / EColor16, and every other depth pins
    // exactly one mode.
    const epoc::display_mode modes[] = {
        epoc::display_mode::gray2, epoc::display_mode::gray4, epoc::display_mode::gray16,
        epoc::display_mode::gray256, epoc::display_mode::color16, epoc::display_mode::color256,
        epoc::display_mode::color4k, epoc::display_mode::color64k, epoc::display_mode::color16m,
        epoc::display_mode::color16ma
    };

    for (const epoc::display_mode mode : modes) {
        const epoc::bitmap_color color = epoc::get_bitmap_color_from_display_mode(mode);
        const int bpp = epoc::get_bpp_from_display_mode(mode);

        REQUIRE(epoc::get_display_mode_from_bpp(bpp, color != epoc::monochrome_bitmap) == mode);
    }

    REQUIRE(epoc::get_bitmap_color_from_display_mode(epoc::display_mode::gray256) == epoc::monochrome_bitmap);
    REQUIRE(epoc::get_bitmap_color_from_display_mode(epoc::display_mode::color256) == epoc::color_bitmap);
    REQUIRE(epoc::get_bitmap_color_from_display_mode(epoc::display_mode::color16ma) == epoc::color_bitmap_with_alpha);
    REQUIRE(epoc::get_bitmap_color_from_display_mode(epoc::display_mode::color16map) == epoc::color_bitmap_with_alpha_pm);
}
