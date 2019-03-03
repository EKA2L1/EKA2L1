/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <cstddef>
#include <cstdint>

namespace eka2l1 {
    /*! \brief Do a 24-bit run-length decompress of the target source buffer.
     *
     * Run length is simple and good, easy, used many in Windows and Symbian source
     * code the old day.
     * 
     * \param src        Pointer to the compressed data. Must not be null.
     * \param src_size   Reference to the size of the compressed data. When decompression done,
     *                   this value is assigned with the number of compressed bytes processed.
     * \param dest       Pointer to the destination buffer. Can be null, if you want to
     *                   count the total decompressed bytes.
     * \param dest_size  Reference contains the maximum size the destination buffer can hold.
     *                   On returns, this value is assigned with the number of decompressed bytes.
    */
    void decompress_rle_24bit(const std::uint8_t *src, std::size_t &src_size,
        std::uint8_t *dest, std::size_t &dest_size);
}
