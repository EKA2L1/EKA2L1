/*
 * Copyright (c) 2026 EKA2L1 Team
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

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1::epoc {
    /**
     * @brief One typeface that link.ini asks to be assembled from others.
     */
    struct linked_font_spec {
        std::u16string name;
        std::vector<std::u16string> component_names;
        std::size_t canonical = 0;
    };

    /**
     * @brief Read a font folder's link.ini, whatever it is encoded in.
     *
     * The file ships as UTF-16LE with a byte order mark; anything else is taken
     * for UTF-8.
     */
    std::u16string decode_linked_font_config(const std::uint8_t *data, const std::size_t size);

    /**
     * @brief Parse link.ini into the typefaces it describes.
     *
     * A line reads, colon separated:
     *
     *   <component typeface>:GROUP<n>:CANONICAL<0|1>:REGULAR:SN<style>:FN<linked typeface>:
     *
     * Lines are grouped by their FN field, which names the typeface being
     * assembled, and keep the order they appear in -- that is the order glyphs
     * are looked up in, and the file lists the Latin component first so that
     * Latin text keeps its own font and everything else falls to the CJK one.
     *
     * The variant sections ([SCHR_LINK_START] and friends) are not honoured:
     * S60 builds the one its feature flags select, while EKA2L1 has no variant
     * flags and instead lets the font store drop the typefaces whose components
     * this ROM does not ship.
     */
    std::vector<linked_font_spec> parse_linked_font_config(const std::u16string &content);
}
