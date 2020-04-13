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
        class process;
        class codeseg;
    }

    using codeseg_ptr = kernel::codeseg *;
    using chunk_ptr = kernel::chunk *;
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

        std::uint8_t *constant_data;
        std::uint8_t *code_data;
    };

    class codeseg : public kernel::kernel_obj {
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

        std::vector<std::uint32_t> export_table;
        std::vector<codeseg_ptr> dependencies;

        epoc::security_info sinfo;

        std::unique_ptr<std::uint8_t[]> constant_data;
        std::unique_ptr<std::uint8_t[]> code_data;

        bool mark{ false };

        struct attached_info {
            kernel::process *attached_process;

            chunk_ptr data_chunk;
            chunk_ptr code_chunk;
        };

        std::vector<attached_info> attaches;

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

        void queries_call_list(kernel::process *pr, std::vector<std::uint32_t> &call_list);
        void unmark();

        bool attach(kernel::process *new_foe);
        bool detatch(kernel::process *de_foe);

        /*! \brief Add new dependency.
        */
        bool add_dependency(codeseg_ptr codeseg);

        /*! \brief Lookup for export.
        */
        address lookup(kernel::process *pr, const std::uint32_t ord);
        address lookup_no_relocate(const std::uint32_t ord);

        void set_full_path(const std::u16string &seg_full_path) {
            full_path = seg_full_path;
        }

        std::u16string get_full_path() {
            return full_path;
        }

        address get_code_run_addr(kernel::process *pr, std::uint8_t **base = nullptr);
        address get_data_run_addr(kernel::process *pr, std::uint8_t **base = nullptr);

        std::uint32_t get_bss_size() const {
            return bss_size;
        }

        std::uint32_t get_code_size() const {
            return code_size;
        }

        std::uint32_t get_data_size() const {
            return data_size;
        }

        address get_code_base() const {
            return code_base;
        }

        address get_data_base() const {
            return data_base;
        }

        bool is_rom() const {
            return code_addr != 0;
        }

        std::uint32_t get_exception_descriptor(kernel::process *pr);
        address get_entry_point(kernel::process *pr);

        epoc::security_info &get_sec_info() {
            return sinfo;
        }

        std::uint32_t export_count() const {
            return static_cast<std::uint32_t>(export_table.size());
        }

        std::tuple<std::uint32_t, std::uint32_t, std::uint32_t> get_uids() {
            return std::make_tuple(uids[0], uids[1], uids[2]);
        }

        std::vector<std::uint32_t> get_export_table(kernel::process *pr);

        // Use for patching
        void set_export(const std::uint32_t ordinal, eka2l1::ptr<void> address);
    };
}