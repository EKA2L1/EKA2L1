/*
 * Copyright (c) 2022 EKA2L1 Team.
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
#include <cstdint>

namespace eka2l1::mem::flexible {
    struct mapping;
    struct memory_object;

    /**
     * @brief Store info about page availbility and theirs protection.
     * 
     * Each 4 bit is reserved to store information about protection of a page. All 0 means that the page
     * is not available for use.
     * 
     * A map can be used to store range information but considering for disconnected chunk case
     * (where memory can be committed randomly), merging range may be complicated. But if any strangers
     * come and want to replace this, cool!
     * 
     * Visit RPageArray class in Symbian as this is a mere copy that is customized to emulator cases.
     */
    struct pages_segment {
    public:    
        static constexpr std::uint32_t PAGE_PER_QWORD = 16;
        static constexpr std::uint32_t PAGE_PER_QWORD_SHIFT = 4;
        static constexpr std::uint32_t QWORD_PER_SEGMENT = 16;
        static constexpr std::uint32_t TOTAL_PAGE_PER_SEGMENT = PAGE_PER_QWORD * QWORD_PER_SEGMENT;
        static constexpr std::uint32_t PAGE_INDEX_MASK_IN_QWORD = PAGE_PER_QWORD - 1;

    private:
        std::uint64_t prots_[QWORD_PER_SEGMENT];

    public:
        explicit pages_segment();
        void alter(const std::uint32_t index, const std::uint32_t count, const prot perm, const bool is_clear);
        void supply_mapping(memory_object *obj, mapping *map, const std::uint32_t segment_index, const std::uint32_t limit);
    };

    struct page_array {
    private:
        pages_segment **segments_;

        std::uint32_t total_pages_;
        std::uint32_t total_segments_;

    public:
        explicit page_array(const std::uint32_t total_pages);
        ~page_array();

        void alter(const std::uint32_t index, const std::uint32_t count, const prot perm, const bool is_clear);
        void supply_mapping(memory_object *obj, mapping *map);
    };
}