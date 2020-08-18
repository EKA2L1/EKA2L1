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
#include <vector>

namespace eka2l1 {
    namespace common {
        class ro_stream;
        class wo_stream;
    }

    /**
     * \brief Compress original data to RLEd.
     * 
     * \param source        Read-only source stream of binary data.
     * \param dest          Write-only destination stream. Can be null for size estimation.
     * \param dest_size     Size of the destination will be written here.
     */
    template <size_t BIT>
    bool compress_rle(common::ro_stream *source, common::wo_stream *dest, std::size_t &dest_size);

    /**
     * \brief Decompress RLE compressed data.
     * 
     * \param source Read-only source stream of binary data.
     * \param dest   Write-only destination stream.
     */
    template <size_t BIT>
    void decompress_rle(common::ro_stream *source, common::wo_stream *dest);
}
