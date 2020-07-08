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

#pragma once

#include <cstdint>
#include <vector>

namespace eka2l1::utils {
    using offset = std::uint64_t;

    class cpp_abi_analyser {
    protected:
        const std::uint8_t *start_;
        const std::uint8_t *end_;

    public:
        explicit cpp_abi_analyser(const std::uint8_t *start, const std::size_t text_size);

        virtual ~cpp_abi_analyser() {}
        
        /**
         * @brief       Search for a vtable of a class, with given clues.
         * 
         * @param       addrs     List of virtual function addresses associated with this class.
         * @returns     On success returns a non-zero offset from the start of .text
         */
        virtual offset search_vtable(std::vector<std::uint32_t> &addrs) = 0;
    };

    class cpp_gcc98_abi_analyser: public cpp_abi_analyser {
    public:
        explicit cpp_gcc98_abi_analyser(const std::uint8_t *start, const std::size_t text_size);
        ~cpp_gcc98_abi_analyser() override {}

        offset search_vtable(std::vector<std::uint32_t> &addrs) override;
    };
}
