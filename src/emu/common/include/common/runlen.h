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

#include <common/buffer.h>

namespace eka2l1 {
    /**
     * \brief Decompress RLE compressed data.
     * 
     * Support only 8-bit aligned compression.
     * 
     * \param source Read-only source stream of binary data.
     * \param dest   Write-only destination stream.
     */
    template <size_t BIT>
    void decompress_rle(common::ro_stream *source, common::wo_stream *dest) {
        static_assert(BIT % 8 == 0, "This RLE decompress function don't support unaligned bit decompress!");

        while (source->valid() && dest->valid()) {
            std::int8_t count8 = 0;
            source->read(&count8, 1);

            std::int32_t count = count8;

            constexpr int BYTE_COUNT = static_cast<int>(BIT) / 8;

            if (count >= 0) {
                count = common::min(count, static_cast<const std::int32_t>(dest->left() / BYTE_COUNT));
                std::uint8_t comp[BYTE_COUNT];

                for (std::size_t i = 0; i < BYTE_COUNT; i++) {
                    source->read(&comp[i], 1);
                }

                while (count >= 0) {
                    for (std::size_t i = 0; i < BYTE_COUNT; i++) {
                        dest->write(&comp[i], 1);
                    }

                    count--;
                }
            } else {
                std::uint32_t num_bytes_to_copy = static_cast<std::uint32_t>(count * -BYTE_COUNT);
                num_bytes_to_copy = common::min(num_bytes_to_copy, static_cast<const std::uint32_t>(dest->left()));
                
                std::vector<std::uint8_t> temp_buf_holder;
                temp_buf_holder.resize(num_bytes_to_copy);

                source->read(&temp_buf_holder[0], num_bytes_to_copy);
                dest->write(temp_buf_holder.data(), num_bytes_to_copy);   
            }
        }
    }
}
