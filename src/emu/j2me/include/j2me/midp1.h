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

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace eka2l1::j2me {
    /**
     * @brief Point a class file's calls to Displayable.getWidth() and getHeight() at Canvas.
     *
     * MIDP 2.0 moved these two methods up from Canvas to Displayable. A MIDP-1.0 game compiled against
     * the MIDP 2.0 classes calls them through Displayable, which the MIDP 1.0 VM of S60v1 does not have:
     * the game stops on the first call with an unhandled FatalSlotError. Canvas has them in both versions.
     *
     * @returns How many method references now name Canvas. A class file this cannot read is left as it is.
     */
    std::size_t redirect_midp2_size_calls(std::vector<char> &class_file);

    /**
     * @brief Copy a JAR for the MIDP 1.0 VM, passing each of its classes through redirect_midp2_size_calls.
     *
     * A JAR that needs no change, or that cannot be read as a zip, is copied byte for byte.
     *
     * @returns False if the copy could not be written.
     */
    bool copy_jar_for_midp1(const std::string &source, const std::string &dest);
}
