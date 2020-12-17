/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#pragma once

#include <cpu/12l1r/common.h>

#include <cstdint>
#include <map>
#include <memory>

namespace eka2l1::arm::r12l1 {
    struct translated_block {
        using hash_type = std::uint64_t;

        hash_type hash_;
        std::uint32_t size_;

        const std::uint8_t *translated_code_;
        std::size_t translated_size_;

        std::uint32_t inst_count_;

        vaddress start_address() const {
            return static_cast<vaddress>(hash_);
        }

        asid address_space() const {
            return static_cast<asid>((hash_ >> 32) & 0xFFFF);
        }

        vaddress current_address() const {
            return static_cast<vaddress>(hash_) + size_;
        }

        explicit translated_block(const vaddress start_addr, const asid aid);
    };

    class block_cache {
        using translated_block_inst = std::unique_ptr<translated_block>;
        std::map<translated_block::hash_type, translated_block_inst> blocks_;

    public:
        explicit block_cache();
        
        bool add_block(const vaddress start_addr, const asid aid);

        // The block that is returned by this is consistent in memory
        translated_block *lookup_block(const vaddress start_addr, const asid aid);

        void flush_range(const vaddress range_start, const vaddress range_end, const asid aid);
        void flush_all();
    };
}