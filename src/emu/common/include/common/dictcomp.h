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

#pragma once

#include <cstdint>

namespace eka2l1::common {
    /*
        Two related source file for this file:
        - https://github.com/SymbianSource/oss.FCL.sf.os.ossrv/blob/1e9520caca186c601dd9768449b86bc72be39a22/lowlevellibsandfws/apputils/inc/BADICTIONARYCOMPRESSION.H
        - https://github.com/SymbianSource/oss.FCL.sf.os.ossrv/blob/1e9520caca186c601dd9768449b86bc72be39a22/lowlevellibsandfws/apputils/src/BADICTIONARYCOMPRESSION.CPP

        This is just a reimplementation by throwing out old-way optimization in the source code and write easy-to-understand code
    */

    /*! \brief A dictionary compress stream
    */
    struct dictcomp {
        int num_bits_used_for_dict_tokens;
        int off_beg;    ///< Begin offset in bit
        int off_cur;    ///< End offset in bit
        int off_end;
        bool owns_bit_buffer;

        std::uint8_t *buffer;

        // Check if the current bit the stream pointing to is 1
        bool is_cur_bit_on();

        explicit dictcomp(std::uint8_t *buf, const int off_beg, const int off_end,
            const int num_bits_used_for_dict_tokens);

        /*! \brief Calculate the size of the buffer, when finish decompressing
         *
         * Size is in bytes.
        */
        int calculate_decompress_size(const bool calypso, bool reset_when_done = true);

        /*! \brief Reading specific number of bit, store it in a stream
        */
        int read_int(int num_bit);

        int index_of_current_directory_entry();

        int read(std::uint8_t *dest, int &dest_size, const bool calypso);
    };
}