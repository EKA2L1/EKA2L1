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

#include <utils/cppabi.h>

namespace eka2l1::utils {
    cpp_abi_analyser::cpp_abi_analyser(const std::uint8_t *start, const std::size_t text_size)
        : start_(start)
        , end_(start_ + text_size) {
    }

    cpp_gcc98_abi_analyser::cpp_gcc98_abi_analyser(const std::uint8_t *start, const std::size_t text_size)
        : cpp_abi_analyser(start, text_size) {
    }

    offset cpp_gcc98_abi_analyser::search_vtable(std::vector<std::uint32_t> &addrs) {
        if (addrs.empty()) {
            // I'm not the great detective. Can you find the vtable with no clues .... 0__0
            return 0;
        }

        // For GCC98 abi, the vtable data comes from the bottom up. Start of a vtable usually begins with 0.
        // For each address, we only search back for some maximum words to avoid infinite loop and segfault
        // (in case some of you poisions this emulator with garbage data)
        static constexpr std::uint32_t MAXIMUM_ADDR_SEARCH_BACK = 1000;
        static constexpr std::uint32_t MAXIMUM_VTABLE_SEARCH_BACK = 100;

        // The addresses that we traced back for each clue must match. Virtual unimplemented method will not have the address of 0.
        offset the_off = 0;
        const std::uint32_t *start_looking = nullptr;

        for (const std::uint32_t addr: addrs) {
            // Looking for this address's word first
            std::uint32_t looked = 0;
            start_looking = reinterpret_cast<const std::uint32_t*>(end_ - 4);

            while ((start_looking >= reinterpret_cast<const std::uint32_t*>(start_)) && (looked <= MAXIMUM_ADDR_SEARCH_BACK)) {
                if (*start_looking == addr) {
                    break;
                }

                looked++;
                start_looking--;
            }

            if (((looked == MAXIMUM_ADDR_SEARCH_BACK) || (start_looking == reinterpret_cast<const std::uint32_t*>(start_))) && (*start_looking != addr)) {
                // Abadon this clue
                continue;
            }

            looked = 0;

            // Begin search back to 0
            while ((start_looking >= reinterpret_cast<const std::uint32_t*>(start_)) && (looked <= MAXIMUM_VTABLE_SEARCH_BACK)) {
                if (*start_looking == 0) {
                    break;
                }

                looked++;
                start_looking--;
            }
            
            if (((looked == MAXIMUM_VTABLE_SEARCH_BACK) || (start_looking == reinterpret_cast<const std::uint32_t*>(start_))) && (*start_looking != 0)) {
                // Abadon this clue, yes
                continue;
            }

            while (*start_looking == 0) {
                start_looking--;
            }

            // Yes because of the loop we do extra subtract
            start_looking++;

            const offset the_new_off = reinterpret_cast<const std::uint8_t*>(start_looking) - start_;

            if ((the_off != 0) && (the_off != the_new_off)) {
                // Count this clue, but must match with previous
                return 0;
            }

            the_off = the_new_off;
        }

        return the_off;
    }
}