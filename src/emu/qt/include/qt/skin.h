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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <common/vecx.h>
#include <drivers/graphics/common.h>

#include <cstdint>
#include <string>
#include <vector>

namespace eka2l1::drivers {
    class graphics_driver;
}

namespace eka2l1::desktop {
    /**
     * \brief A key drawn on a skin.
     */
    struct skin_key {
        std::uint32_t code; ///< Scan code the device receives when the key is touched.
        eka2l1::rect area; ///< Where the key is, in the skin picture.
        eka2l1::rect lit; ///< The key lit, in the lit picture.
        bool ellipse; ///< Touch only inside the ellipse of the area.
    };

    /**
     * \brief Picture of a phone around the screen, with keys that light up and can be touched.
     *
     * Described by a JSON file:
     * {
     *   "image": "phone.png", "lit_image": "phone-lit.png", "size": { "w": 1920, "h": 1080 },
     *   "screen": { "x": 520, "y": 20, "w": 880, "h": 1040 },
     *   "keys": [ { "code": 16, "x": 0, "y": 0, "w": 10, "h": 10, "shape": "rect",
     *               "lit": { "x": 0, "y": 0, "w": 10, "h": 10 } } ]
     * }
     * The pictures are next to the JSON file. "lit" is where the key's lit picture is in
     * the lit picture; it is drawn over the key while the device has the key down.
     */
    struct skin_layout {
        std::string path; ///< The JSON file this layout was read from.
        eka2l1::vec2 size; ///< Size of the picture the positions are given in.
        eka2l1::rect screen; ///< Where the device's screen goes.
        std::vector<skin_key> keys;

        std::string image_path;
        std::string lit_image_path;
        drivers::handle image{ 0 };
        drivers::handle lit_image{ 0 };

        /**
         * \brief Read the layout. Pictures are uploaded later, by the graphics thread.
         * \return False when the file can't be read or has no screen.
         */
        bool load(const std::string &json_path);

        /**
         * \brief Create the textures of both pictures.
         * \return False if the main picture can't be loaded. A missing lit picture only turns lighting off.
         */
        bool upload(drivers::graphics_driver *driver);

        /**
         * \brief Find the key at a point of the skin picture.
         * \return The key, or nullptr.
         */
        const skin_key *key_at(const eka2l1::vec2 &point) const;
    };
}
