/*
 * Copyright (c) 2020 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
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

#include <common/vecx.h>
#include <optional>
#include <vector>

namespace eka2l1::common {
    struct region {
        std::vector<eka2l1::rect> rects_;

        bool empty() const {
            return rects_.empty();
        }

        void make_empty() {
            rects_.clear();
        }

        bool contains(const eka2l1::point &p);

        /**
         * @brief       Move the region by certain amount of units.
         * 
         * @param       amount      The number of X and Y units to move.
         */
        void advance(const eka2l1::vec2 &amount);

        void clip(const eka2l1::rect &bounding);

        /**
         * @brief       Check if two regions are identical in terms of rectangles it contains
         * 
         * Note that if your rectangle are splitted to more smaller part, this can't check that. Hence
         * the identical name here.
         */
        bool identical(const region &lhs) const;

        /**
         * @brief       Add a rectangle to this region.
         * @returns     True if new rectangle create modification to the region.
         */
        bool add_rect(const eka2l1::rect &rect);

        bool add_region(const region &rg);

        /**
         * @brief       Get the rectangle that bound the whole region.
         * @returns     Rectangle that bound the region.
         */
        eka2l1::rect bounding_rect() const;

        /**
         * @brief   Get intersection between two regions.
         * 
         * @param   target    The region to get intersection with.
         * @returns The region that intersects.
         */
        region intersect(const region &target) const;

        /**
         * @brief Remove a rectangle from this region.
         * 
         * @param rect  The rectangle to remove from this region.
         */
        void eliminate(const eka2l1::rect &rect);

        /**
         * @brief Remove another region that intersects or inside this region from this region.
         * 
         * @param reg   The region to remove from.
         */
        void eliminate(const region &reg);
    };
}