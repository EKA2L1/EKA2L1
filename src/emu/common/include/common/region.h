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

        /**
         * @brief       Add a rectangle to this region.
         * @returns     True if new rectangle create modification to the region.
         */
        bool add_rect(const eka2l1::rect &rect);

        /**
         * @brief       Get the rectangle that bound the whole region.
         * @returns     Rectangle that bound the region.
         */
        eka2l1::rect bounding_rect() const;

        /**
         * @brief   Check intersection rectangle between a rectangle and this region.
         * 
         * @param   target    The rectangle to check intersection with.
         * @returns True if intersects.
         */
        bool intersects(const eka2l1::rect &target) const;

        /**
         * @brief Remove a rectangle from this region.
         * 
         * @param rect  The rectangle to remove from this region.
         */
        void eliminate(const eka2l1::rect &rect);
    };
}