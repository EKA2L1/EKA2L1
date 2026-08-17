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

// What Symbian app icons need from the vendored lunasvg, pinned down so a bump
// that drops either capability is caught here rather than by a blank icon on a
// device. Both cases come from real MIF icons: several S60v3 games store their
// artwork as a base64 PNG inside the SVG, and the ones exported from Adobe wrap
// that image in <switch><foreignObject>.

#include <lunasvg.h>

#include <catch2/catch.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace {
    // A 2x2 opaque red PNG.
    constexpr const char *RED_PNG_BASE64 =
        "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAEUlEQVR4nGP4z8DwH4QZYAwAR8oH+WdZbrcAAAAASUVORK5CYII=";

    std::vector<std::uint8_t> render(const std::string &svg, const int side) {
        std::unique_ptr<lunasvg::Document> doc = lunasvg::Document::loadFromData(svg);
        REQUIRE(doc != nullptr);

        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(side) * side * 4, 0);
        lunasvg::Bitmap bitmap(rgba.data(), side, side, side * 4);

        const float sx = (doc->width() > 0) ? (static_cast<float>(side) / doc->width()) : 1.0f;
        const float sy = (doc->height() > 0) ? (static_cast<float>(side) / doc->height()) : 1.0f;

        doc->render(bitmap, lunasvg::Matrix{ sx, 0, 0, sy, 0, 0 });
        bitmap.convertToRGBA();

        return rgba;
    }

    bool has_opaque_pixel(const std::vector<std::uint8_t> &rgba) {
        for (std::size_t i = 3; i < rgba.size(); i += 4) {
            if (rgba[i] != 0) {
                return true;
            }
        }

        return false;
    }
}

TEST_CASE("svg_renders_a_data_uri_image", "svg_icon") {
    const std::string svg = std::string(
                                "<svg xmlns=\"http://www.w3.org/2000/svg\" "
                                "xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"8\" height=\"8\">"
                                "<image width=\"8\" height=\"8\" xlink:href=\"data:image/png;base64,")
        + RED_PNG_BASE64 + "\"/></svg>";

    REQUIRE(has_opaque_pixel(render(svg, 8)));
}

TEST_CASE("svg_renders_through_a_switch_element", "svg_icon") {
    // lunasvg skips an element it does not know together with its whole subtree,
    // so without <switch> support the image inside is never drawn.
    const std::string svg = std::string(
                                "<svg xmlns=\"http://www.w3.org/2000/svg\" "
                                "xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"8\" height=\"8\">"
                                "<switch><foreignObject width=\"8\" height=\"8\"/>"
                                "<image width=\"8\" height=\"8\" xlink:href=\"data:image/png;base64,")
        + RED_PNG_BASE64 + "\"/></switch></svg>";

    REQUIRE(has_opaque_pixel(render(svg, 8)));
}
