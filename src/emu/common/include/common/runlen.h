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

    /**
     * @brief Decompress RLE compressed data in memory.
     * 
     * @param source            Read-only source buffer pointer.
     * @param souce_size        The size of the source buffer.
     * @param dest              The destination buffer to extract data to. Can be NULL.
     * @param dest_size         Size of the destination buffer, on complete the parameter will be filled with the number of bytes decompressed.
     */
    template <size_t BIT>
    void decompress_rle_fast_route(const std::uint8_t *source, const std::size_t source_size, std::uint8_t *dest, std::size_t &dest_size);
}
