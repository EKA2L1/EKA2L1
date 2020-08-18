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
#include <common/buffer.h>
#include <common/log.h>
#include <common/runlen.h>

namespace eka2l1 {
    template <size_t BIT>
    bool compress_rle(common::ro_stream *source, common::wo_stream *dest, std::size_t &dest_size) {
        static_assert(BIT % 8 == 0, "This RLE compress function don't support unaligned bit decompress!");
        dest_size = 0;

        while (source->valid() && (!dest || dest->valid())) {
            // Read 3 bytes
            static constexpr std::int32_t BYTE_COUNT = BIT / 8;

            std::uint8_t bytes[BYTE_COUNT];
            std::uint64_t last_position = source->tell();

            if (source->read(bytes, BYTE_COUNT) != BYTE_COUNT) {
                return false;
            }

            std::uint8_t temp_bytes[BYTE_COUNT];

            if ((source->read(temp_bytes, BYTE_COUNT) == BYTE_COUNT) && std::equal(bytes, bytes + BYTE_COUNT, temp_bytes)) {
                // Seek back BYTE_COUNT in prepare of the do while loop
                source->seek(-BYTE_COUNT, common::seek_where::cur);

                do {
                    source->read(temp_bytes, static_cast<std::uint64_t>(BYTE_COUNT));
                } while (source->valid() && std::equal(bytes, bytes + BYTE_COUNT, temp_bytes));

                // Seek back again, to evade the unequal sequence
                if (source->valid()) {
                    source->seek(-BYTE_COUNT, common::seek_where::cur);
                }

                const std::uint64_t total_bytes_repeated = source->tell() - last_position;
                assert(total_bytes_repeated % BYTE_COUNT == 0);

                std::int64_t total_pair = static_cast<std::int64_t>(total_bytes_repeated / BYTE_COUNT);

                while (total_pair > 0) {
                    std::uint8_t total_this_session = (total_pair > 128) ? 127 : static_cast<std::uint8_t>(total_pair - 1);

                    if (dest) {
                        dest->write(&total_this_session, 1);
                        dest->write(bytes, BYTE_COUNT);
                    }

                    dest_size += BYTE_COUNT + 1;
                    total_pair -= (total_this_session + 1);
                }
            } else {
                // Seek back BYTE_COUNT in prepare of the do while loop
                source->seek(-BYTE_COUNT, common::seek_where::cur);

                bool should_copy = false;

                do {
                    if (should_copy)
                        std::copy(temp_bytes, temp_bytes + BYTE_COUNT, bytes);

                    source->read(temp_bytes, static_cast<std::uint64_t>(BYTE_COUNT));
                    should_copy = true;
                } while (source->valid() && !std::equal(bytes, bytes + BYTE_COUNT, temp_bytes));

                if (source->valid()) {
                    source->seek(-BYTE_COUNT, common::seek_where::cur);
                }

                const std::uint64_t total_bytes_repeated = source->tell() - last_position;
                std::int64_t total_pair = static_cast<std::int64_t>(total_bytes_repeated / BYTE_COUNT);

                if (dest) {
                    source->seek(last_position, common::seek_where::beg);
                }

                std::vector<std::uint8_t> temp_data;

                while (total_pair > 0) {
                    std::int8_t total_this_session = (total_pair > 128) ? -128 : static_cast<std::int8_t>(-total_pair);

                    if (dest) {
                        dest->write(&total_this_session, 1);

                        // Read the source
                        temp_data.resize(total_this_session * -BYTE_COUNT);
                        source->read(&temp_data[0], temp_data.size());
                        dest->write(&temp_data[0], static_cast<std::uint32_t>(temp_data.size()));
                    }

                    dest_size += 1 + static_cast<std::size_t>(total_this_session * -BYTE_COUNT);

                    // Negative already!
                    total_pair += total_this_session;
                }
            }
        }

        return true;
    }

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

    template <>
    void decompress_rle<12>(common::ro_stream *source, common::wo_stream *dest) {
        while (source->valid() && dest->valid()) {
            std::uint16_t val = 0;
            if (source->read(&val, 2) != 2) {
                return;
            }

            std::uint32_t repeat_count = common::min<std::uint32_t>(static_cast<std::uint32_t>((val >> 12) & 0xF),
                static_cast<const std::uint32_t>(dest->left() / 2));

            val &= 0x0FFF;

            for (std::uint32_t i = 0; i <= repeat_count; i++) {
                dest->write(&val, 2);
            }
        }
    }

    template bool compress_rle<8>(common::ro_stream *source, common::wo_stream *dest, std::size_t &dest_size);
    template bool compress_rle<16>(common::ro_stream *source, common::wo_stream *dest, std::size_t &dest_size);
    template bool compress_rle<24>(common::ro_stream *source, common::wo_stream *dest, std::size_t &dest_size);
    template bool compress_rle<32>(common::ro_stream *source, common::wo_stream *dest, std::size_t &dest_size);

    template void decompress_rle<8>(common::ro_stream *source, common::wo_stream *dest);
    template void decompress_rle<16>(common::ro_stream *source, common::wo_stream *dest);
    template void decompress_rle<24>(common::ro_stream *source, common::wo_stream *dest);
    template void decompress_rle<32>(common::ro_stream *source, common::wo_stream *dest);
}
