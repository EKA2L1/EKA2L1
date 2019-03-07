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
#include <common/log.h>
#include <common/runlen.h>

namespace eka2l1 {
    void decompress_rle_24bit(const std::uint8_t *src, std::size_t &src_size,
        std::uint8_t *dest, std::size_t &dest_size) {
        const std::uint8_t *src_org = src;
        const std::uint8_t *dest_org = dest;
        
        const std::uint8_t *src_end = src + src_size;
        const std::uint8_t *dest_end = dest + dest_size;

        while (src < src_end && (dest_org ? dest < dest_end : true)) {
            std::int32_t count = static_cast<std::int8_t>(*src++);

            if (count >= 0) {
                if (dest_org) {
                    count = common::min(count, static_cast<std::int32_t>((dest_end - dest) / 3));
                }

                const std::uint8_t comp1 = *src++;
                const std::uint8_t comp2 = *src++;
                const std::uint8_t comp3 = *src++;

                if (dest_org) {
                    while (count >= 0) {
                        *dest++ = comp1;
                        *dest++ = comp2;
                        *dest++ = comp3;

                        count--;
                    }
                } else {
                    dest += (count + 1) * 3;
                }
            } else {
                std::uint32_t num_bytes_to_copy = count * -3;

                if (dest_org) {
                    num_bytes_to_copy = common::min(num_bytes_to_copy, static_cast<const std::uint32_t>(dest_end - dest));
                    std::copy(src, src + num_bytes_to_copy, dest);
                }

                src += num_bytes_to_copy;
                dest += num_bytes_to_copy;
            }
        }

        dest_size = dest - dest_org;
        src_size = src - src_org;
    }
    
    void decompress_rle_24bit_stream(common::ro_stream *stream, std::size_t &src_size, std::uint8_t *dest, std::size_t &dest_size) {
        const std::uint8_t *dest_org = dest;
        const std::uint8_t *dest_end = dest + dest_size;

        const std::size_t src_org = src_size;
        const std::size_t crr_pos = stream->tell();

        while (stream->valid() && (dest_org ? dest < dest_end : true)) {
            std::int8_t count8 = 0;
            stream->read(&count8, 1);

            std::int32_t count = count8;

            if (count >= 0) {
                if (dest_org) {
                    count = common::min(count, static_cast<std::int32_t>((dest_end - dest) / 3));
                }

                std::uint8_t comp1 = 0;
                std::uint8_t comp2 = 0;
                std::uint8_t comp3 = 0;

                stream->read(&comp1, 1);
                stream->read(&comp2, 1);
                stream->read(&comp3, 1);

                if (dest_org) {
                    while (count >= 0) {
                        *dest++ = comp1;
                        *dest++ = comp2;
                        *dest++ = comp3;

                        count--;
                    }
                } else {
                    dest += (count + 1) * 3;
                }
            } else {
                std::uint32_t num_bytes_to_copy = count * -3;

                if (dest_org) {
                    num_bytes_to_copy = common::min(num_bytes_to_copy, static_cast<const std::uint32_t>(dest_end - dest));
                    stream->read(dest, num_bytes_to_copy);
                } else {
                    stream->seek(num_bytes_to_copy, common::seek_where::cur);
                }

                dest += num_bytes_to_copy;
            }
        }

        dest_size = dest - dest_org;
        src_size = stream->tell() - crr_pos;
    }
}
