/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <epoc/kernel/kernel_obj.h>
#include <epoc/ptr.h>

#include <vector>

namespace eka2l1::kernel {
    struct codeseg_create_info {
        std::uint32_t uids[3];
        std::uint32_t code_base;
        std::uint32_t data_base;
        
        std::uint32_t code_size;
        std::uint32_t data_size;
        std::uint32_t bss_size;
        
        address &code_load_addr;
        address &data_load_addr;

        // Offset from code address
        std::uint32_t entry_point;

        std::vector<std::uint32_t> export_table;
    };

    class codeseg: public kernel::kernel_obj {
        std::uint32_t uids[3];

        std::uint32_t code_addr;
        std::uint32_t data_addr;

        std::uint32_t code_base;
        std::uint32_t data_base;

        std::uint32_t code_size;
        std::uint32_t data_size;
        std::uint32_t bss_size;

        std::uint32_t ep;

        // Invalid handle if those are not available
        handle code_chunk;
        handle data_chunk;

        std::vector<std::uint32_t> export_table;

    public:
        /*! \brief Create a new codeseg
         *
         * code_load_addr and data_load_addr will be replace with load address.
         * 
         * Ideally, apps can use this to create a code portion that will resides in RAM code address.
         * Don't pass relocation table, export table, and import table, no data size and only code base with random numbers.
         * UID should be unique too, which you can choose any numbers that is far from 0x20000000 (UID0). After that, call 
         * CodesegCreate and the code_addr should be return for you.
        */
        explicit codeseg(kernel_system *kern, const std::string &name,
            codeseg_create_info &info);

        /*! \brief Open the codeseg
         *
         * This changes the code chunk perm to read_write_exec.
         * TODO: Check if it should be read_exec. Some app modifies ROM shadow ?
         * 
         * \param pr_id ID of the process that opens this codeseg
        */
        bool open(const std::uint32_t pr_id);

        /*! \brief Close codeseg
        */
        bool close();

        address lookup(const std::uint8_t ord);
    };
}