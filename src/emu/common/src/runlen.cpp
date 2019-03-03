/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <common/algorithm.h>
#include <common/runlen.h>

namespace eka2l1 {
    void decompress_rle_24bit(const std::uint8_t *src, std::size_t &src_size,
        std::uint8_t *dest, std::size_t &dest_size) {
        const std::uint8_t *src_org = src;
        const std::uint8_t *dest_org = dest;
        
        const std::uint8_t *src_end = src + src_size;
        const std::uint8_t *dest_end = dest + dest_size;

        while (src < src_end && dest < dest_end) {
            std::uint8_t count = *src;

            if (count >= 0) {
                count = common::min(count, static_cast<std::uint8_t>((dest_end - dest) / 3));

                const std::uint8_t comp1 = *src++;
                const std::uint8_t comp2 = *src++;
                const std::uint8_t comp3 = *src++;

                while (count >= 0) {
                    *dest++ = comp1;
                    *dest++ = comp2;
                    *dest++ = comp3;

                    count--;
                }
            } else {
                const std::uint32_t num_bytes_to_copy = common::min(
                    static_cast<const std::uint32_t>(count * -3), 
                    static_cast<const std::uint32_t>(dest_end - dest));

                std::copy(src, src + num_bytes_to_copy, dest);

                src += num_bytes_to_copy;
                dest += num_bytes_to_copy;
            }
        }

        dest_size = dest - dest_org;
        src_size = src - src_org;
    }
}
