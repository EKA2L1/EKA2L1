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

#include <epoc/kernel/codeseg.h>
#include <epoc/kernel.h>
#include <algorithm>

namespace eka2l1::kernel {
    codeseg::codeseg(kernel_system *kern, const std::string &name,
            codeseg_create_info &info)
        : kernel_obj(kern, name, kernel::access_type::global_access)
        , code_chunk(INVALID_HANDLE)
        , data_chunk(INVALID_HANDLE) {
        std::copy(info.uids, info.uids + 3, uids);
        code_base = info.code_base;
        data_base = info.data_base;

        code_size = info.code_size;
        data_size = info.data_size;
        bss_size = info.bss_size;

        ep = info.entry_point;

        export_table = std::move(info.export_table);

        // Only allows write when it's not open yet
        code_chunk = kern->create_chunk(name, 0, code_size, code_size, prot::read_write, kernel::chunk_type::normal,
            kernel::chunk_access::code, kernel::chunk_attrib::none, kernel::owner_type::kernel);

        if (data_size) {
            data_chunk = kern->create_chunk(name, 0, data_size, code_size, prot::read_write, kernel::chunk_type::normal,
                kernel::chunk_access::code, kernel::chunk_attrib::none, kernel::owner_type::kernel);
        }

        code_addr = std::reinterpret_pointer_cast<kernel::chunk>(kern->get_kernel_obj(code_chunk))->base().ptr_address();
        info.code_load_addr = code_addr;

        if (data_size) {
            auto data_chunk_obj = std::reinterpret_pointer_cast<kernel::chunk>(kern->get_kernel_obj(data_chunk));
            data_addr = data_chunk_obj->base().ptr_address();
            std::uint8_t *bss = data_chunk_obj->base().get(kern->get_memory_system());

            // Initialize bss
            std::fill(bss, bss + bss_size, 0);

            info.data_load_addr = data_addr;
        }
    }

    bool codeseg::open(const std::uint32_t id) {
        int err = kern->get_memory_system()->change_prot(code_addr, code_size, prot::read_exec);
        return !err;
    }
    
    bool codeseg::close() {
        int err = kern->get_memory_system()->change_prot(code_addr, code_size, prot::read_exec);
        return !err;
    }
    
    address codeseg::lookup(const std::uint8_t ord) {
        if (ord > export_table.size()) {
            return 0;
        }

        return export_table[ord] + code_addr;
    }
}