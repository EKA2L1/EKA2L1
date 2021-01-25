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

#include <common/types.h>
#include <cpu/12l1r/common.h>

#include <algorithm>
#include <cstdint>

namespace eka2l1::arm::r12l1 {
    struct tlb_entry {
        vaddress read_addr;
        vaddress write_addr;
        vaddress execute_addr;

        std::uint8_t *host_base;

        explicit tlb_entry()
            : read_addr(0)
            , write_addr(0)
            , execute_addr(0)
            , host_base(nullptr) {
        }
    };

    static constexpr std::uint32_t TLB_LOOKUP_BIT_COUNT = 9;
    static constexpr std::uint32_t TLB_ENTRY_COUNT = 1 << TLB_LOOKUP_BIT_COUNT;
    static constexpr std::uint32_t TLB_ENTRY_MASK = TLB_ENTRY_COUNT - 1;

    struct tlb {
    public:
        tlb_entry entries[TLB_ENTRY_COUNT];

        std::size_t page_bits;
        std::size_t page_mask;

        explicit tlb(std::size_t page_bits)
            : page_bits(page_bits) {
            page_mask = (1 << page_bits) - 1;
            flush();
        }

        void flush() {
            // Using memfill to speed up this process
            std::memset(entries, 0, sizeof(tlb_entry) * TLB_ENTRY_COUNT);
        }

        void add(vaddress addr, std::uint8_t *host, const std::uint32_t perm) {
            const std::size_t page_index = addr >> page_bits;
            const std::size_t tlb_index = page_index & (TLB_ENTRY_COUNT - 1);
            const std::size_t addr_mod = addr & page_mask;
            const vaddress addr_normed = addr & ~page_mask;

            tlb_entry &entry = entries[tlb_index];
            entry.host_base = host - addr_mod;

            if (perm & prot_read) {
                entry.read_addr = addr_normed;
            } else {
                entry.read_addr = 0;
            }

            if (perm & prot_write) {
                entry.write_addr = addr_normed;
            } else {
                entry.write_addr = 0;
            }

            if (perm & prot_exec) {
                entry.execute_addr = addr_normed;
            } else {
                entry.execute_addr = 0;
            }
        }

        void make_dirty(const vaddress addr) {
            const std::size_t page_index = addr >> page_bits;
            const std::size_t tlb_index = page_index & (TLB_ENTRY_COUNT - 1);
            const vaddress addr_normed = addr & ~page_mask;

            tlb_entry &entry = entries[tlb_index];

            if ((entry.read_addr == addr_normed) || (entry.write_addr == addr_normed) || (entry.execute_addr == addr_normed)) {
                std::memset(&entry, 0, sizeof(tlb_entry));
            }
        }

        std::uint8_t *lookup(const vaddress addr) {
            const std::size_t page_index = addr >> page_bits;
            const std::size_t tlb_index = page_index & (TLB_ENTRY_COUNT - 1);
            const vaddress addr_normed = addr & ~page_mask;

            tlb_entry &entry = entries[tlb_index];

            if (!entry.host_base) {
                return nullptr;
            }

            if ((entry.read_addr == addr_normed) || (entry.write_addr == addr_normed) || (entry.execute_addr == addr_normed)) {
                const std::size_t addr_mod = addr & page_mask;
                return entry.host_base + addr_mod;
            }

            // TLB miss
            return nullptr;
        }
    };
}