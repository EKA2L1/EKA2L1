/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <limits>
#include <random>

/*! \brief Namespace contains all emulator code. */
namespace eka2l1 {
    /*! \brief Generate a random unsigned 32 bit number.
	 *
	 * \returns The random number generated.
	*/
    uint32_t random();

    /*! \brief Generate a random unsigned 32 bit number, in a range.
     *
     * \param beg, end The range to generate number in.
     * \returns The random number generated.
    */
    uint32_t random_range(uint32_t beg, uint32_t end);
}
