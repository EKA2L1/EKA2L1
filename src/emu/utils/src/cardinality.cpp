/*
 * Copyright (c) 2020 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
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

#include <common/buffer.h>
#include <utils/cardinality.h>

namespace eka2l1::utils {
    bool cardinality::internalize(common::ro_stream &stream) {
        std::uint8_t b1 = 0;
        std::uint32_t len = 0;

        if (stream.read(&b1, 1) != 1) {
            return false;
        }

        if ((b1 & 1) == 0) {
            len = (b1 >> 1);
        } else {
            if ((b1 & 2) == 0) {
                len = b1;

                if (stream.read(&b1, 1) != 1) {
                    return false;
                }

                len += b1 << 8;
                len >>= 2;
            } else if ((b1 & 4) == 0) {
                std::uint16_t b2 = 0;

                if (stream.read(&b2, 2) != 2) {
                    return false;
                }

                len = b1 + (b2 << 8);
                len >>= 4;
            }
        }

        val_ = len;
        return true;
    }
}