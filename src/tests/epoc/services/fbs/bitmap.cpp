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

#include <services/fbs/bitmap.h>

#include <catch2/catch.hpp>

#include <cstdint>
#include <vector>

using namespace eka2l1;

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

TEST_CASE("icon_mask_soft_maps_grey_to_alpha", "icon_mask") {
    // Three levels, no solid white border: a real alpha ramp, so the mask reads
    // as a soft mask and its grey level becomes the icon's alpha directly.
    std::vector<std::uint8_t> levels(ICON_SIDE * ICON_SIDE, 128);
    levels[0] = 0;
    levels[1] = 255;

    std::vector<std::uint8_t> icon = make_icon();
    const std::vector<std::uint8_t> mask = make_mask(levels);

    epoc::apply_icon_mask_alpha(icon.data(), mask.data(), ICON_SIDE, ICON_SIDE, 8);

    REQUIRE(alpha_at(icon, 0) == 0);
    REQUIRE(alpha_at(icon, 1) == 255);
    REQUIRE(alpha_at(icon, 2) == 128);
}

TEST_CASE("icon_mask_colour_key_inverts_the_white_backdrop", "icon_mask") {
    // A colour-key mask is stored at a colour depth, and make_standard_mask has
    // put "this pixel is the white backdrop" in the alpha channel. Transparency
    // is therefore the inverse of that.
    std::vector<std::uint8_t> levels(ICON_SIDE * ICON_SIDE, 0);
    levels[0] = 255;

    std::vector<std::uint8_t> icon = make_icon();
    const std::vector<std::uint8_t> mask = make_mask(levels);

    epoc::apply_icon_mask_alpha(icon.data(), mask.data(), ICON_SIDE, ICON_SIDE, 16);

    REQUIRE(alpha_at(icon, 0) == 0);
    REQUIRE(alpha_at(icon, 1) == 255);
}

TEST_CASE("icon_mask_binary_stencil_at_low_depth_is_not_soft", "icon_mask") {
    // The S60v2 case this exists for: a *binary* stencil stored at 8bpp, in the
    // colour-key polarity. Reading it as a soft mask turns the icon inside out --
    // the artwork goes transparent and the backdrop stays opaque. A mask with only
    // white plus one other level, a solid white border and a non-white interior
    // cannot be anything but colour-key.
    std::vector<std::uint8_t> levels(ICON_SIDE * ICON_SIDE, 255);
    levels[ICON_SIDE + 1] = 0;
    levels[ICON_SIDE + 2] = 0;
    levels[2 * ICON_SIDE + 1] = 0;
    levels[2 * ICON_SIDE + 2] = 0;

    std::vector<std::uint8_t> icon = make_icon();
    const std::vector<std::uint8_t> mask = make_mask(levels);

    epoc::apply_icon_mask_alpha(icon.data(), mask.data(), ICON_SIDE, ICON_SIDE, 8);

    // The white border is the backdrop: transparent. The interior is the artwork.
    REQUIRE(alpha_at(icon, 0) == 0);
    REQUIRE(alpha_at(icon, ICON_SIDE + 1) == 255);
}

TEST_CASE("icon_mask_full_bleed_mask_stays_soft", "icon_mask") {
    // An entirely white mask is a full-bleed icon, not a stencil: every pixel is
    // opaque. Classifying it as colour-key would erase the icon.
    const std::vector<std::uint8_t> levels(ICON_SIDE * ICON_SIDE, 255);

    std::vector<std::uint8_t> icon = make_icon();
    const std::vector<std::uint8_t> mask = make_mask(levels);

    epoc::apply_icon_mask_alpha(icon.data(), mask.data(), ICON_SIDE, ICON_SIDE, 8);

    for (std::size_t i = 0; i < ICON_SIDE * ICON_SIDE; i++) {
        REQUIRE(alpha_at(icon, i) == 255);
    }
}

TEST_CASE("icon_mask_ignores_missing_buffers", "icon_mask") {
    std::vector<std::uint8_t> icon = make_icon();
    const std::vector<std::uint8_t> mask = make_mask(std::vector<std::uint8_t>(ICON_SIDE * ICON_SIDE, 128));

    epoc::apply_icon_mask_alpha(nullptr, mask.data(), ICON_SIDE, ICON_SIDE, 8);
    epoc::apply_icon_mask_alpha(icon.data(), nullptr, ICON_SIDE, ICON_SIDE, 8);

    REQUIRE(alpha_at(icon, 0) == 255);
}
