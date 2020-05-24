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
#include <epoc/mem/common.h>

namespace eka2l1::mem::flexible {
    struct memory_object;
    struct address_space;

    /**
     * @brief Represent a memory object being mapped to a process.
     * 
     * Each memory object mapped to a process has different base. This object basically
     * is a description of that mapping.
     */
    struct mapping {
        vm_address base_;
        address_space *owner_;

        std::size_t occupied_;

    public:
        explicit mapping(address_space *owner);
        ~mapping();

        bool instantiate(const std::size_t page_occupied, const std::uint32_t flags, const vm_address forced);

        bool map(memory_object *obj, const std::uint32_t index, const std::size_t count, const prot permissions);
        bool unmap(const std::uint32_t index_start, const std::size_t count);
    };
}