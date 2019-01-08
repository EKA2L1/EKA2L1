/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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
#include <common/dictcomp.h>
#include <common/log.h>

namespace eka2l1::common {
    bool dictcomp::is_cur_bit_on() {
        return buffer[off_cur / 8] & (1 << (off_cur) % 8);
    }

    int dictcomp::index_of_current_directory_entry() {
        if (is_cur_bit_on()) {
            ++off_cur;
            return read_int(num_bits_used_for_dict_tokens);
        }

        return -1;
    }

    int dictcomp::read_int(int num_bit) {
        int result = 0;

        auto do_read_bits_seperately = [&]() {
            // If the offset is not aligned, we can't read this properly
            // We need to do some bits tweaking, to read these byte to an integer, then align the bit offset to 8
            if (off_cur % 8 != 0) {
                int num_bit_to_read_next = common::min(num_bit, 8 - off_cur % 8);
                char me = buffer[off_cur % 8];

                // Example:
                // 0b11110000, from bit 1, size = 3
                // => 0b11100000
                // 0b11111111 << 5 = 0b11100000
                // => 0b11100000 & 0b11100000 => 0b111
                result = (result << 8) | (me << (off_cur % 8) & (0xFF << (8 - num_bit_to_read_next)));

                off_cur += num_bit_to_read_next;
                num_bit -= num_bit_to_read_next;
            }
        };

        // Do align if neccessary
        do_read_bits_seperately();

        if (num_bit <= 0) {
            return result;
        }

        // Read byte by byte
        for (int i = 0; i < num_bit / 8; i++) {
            result = (result << 8) | buffer[off_cur / 8];
            off_cur += 8;
        }

        if (num_bit <= 0) {
            return result;
        }

        // Try to read left-out bits, if we haven't read them all
        do_read_bits_seperately();
        
        return result;
    }
    
    int dictcomp::calculate_decompress_size(const bool calypso, bool reset_when_done) {
        int last_off_cur = off_cur;
        int num_consecutive_prefix_bits = 0;

        for (std::uint8_t i = 0; i < 4; i++) {
            bool cur_bit_on = is_cur_bit_on();
            off_cur++;

            if (!cur_bit_on) {
                break;
            }

            num_consecutive_prefix_bits++;
        }

        int num_bytes_to_read = 0;
        
        if (num_consecutive_prefix_bits == 3) {
            num_bytes_to_read = 3 + read_int(3);
        } else if (num_consecutive_prefix_bits == 4) {
            num_bytes_to_read = 4 + read_int(8);

            if (!calypso) {
                num_bytes_to_read = 3 + (1 << 3);
            }
        } else {
            num_bytes_to_read = num_consecutive_prefix_bits;
        }

        if (reset_when_done) {
            off_cur = last_off_cur;
        }

        return num_bytes_to_read;
    }

    bool dictcomp::read(std::uint8_t *dest, int &dest_size, const bool calypso) {
        // These size in bytes
        int size = calculate_decompress_size(calypso, false);
        int *dest_i = reinterpret_cast<int*>(dest);

        if (size > dest_size) {
            LOG_ERROR("Can't decompress: unsufficent memory (needed: 0x{:X} vs provided 0x{:X})", size, dest_size);
            return false;
        }

        const int num_bits_off_byte_bound = off_cur % 8;
        const std::uint8_t *cur_byte = buffer + (off_cur / 8);

        for (int i = 0; i < size; i++, ++cur_byte) {
            int b = *cur_byte;

            if (num_bits_off_byte_bound > 0) {
                b >>= num_bits_off_byte_bound;
                b |= (*(cur_byte) + 1) << (8 - num_bits_off_byte_bound);
                b &= 0xFF;
            }

            *(dest_i++) = b;
        }

        off_cur += size * 8;
        return true;
    }
}