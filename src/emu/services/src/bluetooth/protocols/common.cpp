/*
 * Copyright (c) 2022 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project.
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

#include <services/bluetooth/protocols/common.h>
#include <cstring>

namespace eka2l1::epoc::bt {
    static const std::uint8_t BLUETOOTH_BASE_UUID[] = {
        0x00, 0x00, 0x10, 0x00, 0x80, 0x00,0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
	};

    const std::uint8_t *uuid::shortest_form(std::uint32_t &size) const {
        if (std::memcmp(data_ + 4, BLUETOOTH_BASE_UUID, 12) == 0) {
            size = 4;
        }

        if ((size == 4) && *reinterpret_cast<const std::uint16_t*>(data_) == 0) {
            size = 2;
            return data_ + 2;
        }

        return data_;
    }
}