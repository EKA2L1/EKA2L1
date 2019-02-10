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

#pragma once

#include <epoc/kernel/kernel_obj.h>
#include <epoc/ptr.h>
#include <epoc/utils/sec.h>

#include <tuple>
#include <vector>

namespace eka2l1 {
    namespace kernel {
        class chunk;
        class codeseg;
    }
    
    using codeseg_ptr = std::shared_ptr<kernel::codeseg>;
    using chunk_ptr = std::shared_ptr<kernel::chunk>;
}

namespace eka2l1::kernel {
    struct codeseg_create_info {
        std::u16string full_path;

        std::uint32_t uids[3];
        std::uint32_t code_base = 0;
        std::uint32_t data_base = 0;
        
        std::uint32_t code_size = 0;
        std::uint32_t data_size = 0;
        std::uint32_t bss_size = 0;

        // Offset to exception descriptor
        std::uint32_t exception_descriptor = 0;
        
        address code_load_addr = 0;
        address data_load_addr = 0;

        // Offset from code address
        std::uint32_t entry_point = 0;

        std::vector<std::uint32_t> export_table;
        epoc::security_info sinfo;
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
        std::uint32_t exception_descriptor;

        // Full path (if there is)
        std::u16string full_path;

        // Invalid handle if those are not available
        chunk_ptr code_chunk;
        chunk_ptr data_chunk;

        std::vector<std::uint32_t> export_table;
        std::vector<codeseg_ptr>   dependencies;

        epoc::security_info sinfo;

        bool mark { false };

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

        virtual ~codeseg() {}

        void queries_call_list(std::vector<std::uint32_t> &call_list);

        /*! \brief Add new dependency.
        */
        bool add_dependency(codeseg_ptr codeseg);

        /*! \brief Lookup for export.
        */
        address lookup(const std::uint32_t ord);

        void set_full_path(const std::u16string &seg_full_path) {
            full_path = seg_full_path;
        }

        std::u16string get_full_path() {
            return full_path;
        }

        address get_code_run_addr() const {
            return code_addr;
        }

        address get_data_run_addr() const {
            return data_addr;
        }

        std::uint32_t get_bss_size() const {
            return bss_size;
        }

        std::uint32_t get_code_size() const {
            return code_size;
        }
        
        std::uint32_t get_data_size() const {
            return data_size;
        }

        std::uint32_t get_exception_descriptor() const {
            return exception_descriptor;
        }

        address get_entry_point() {
            return ep;
        }

        epoc::security_info &get_sec_info() {
            return sinfo;
        }

        std::tuple<std::uint32_t, std::uint32_t, std::uint32_t> get_uids() {
            return std::make_tuple(uids[0], uids[1], uids[2]);
        }

        std::vector<std::uint32_t> &get_export_table() {
            return export_table;
        }
    };
}