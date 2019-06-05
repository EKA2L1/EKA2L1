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

#include <algorithm>
#include <epoc/kernel.h>
#include <epoc/kernel/codeseg.h>

namespace eka2l1::kernel {
    codeseg::codeseg(kernel_system *kern, const std::string &name,
        codeseg_create_info &info)
        : kernel_obj(kern, name, kernel::access_type::global_access)
        , code_chunk(nullptr)
        , data_chunk(nullptr) {
        std::copy(info.uids, info.uids + 3, uids);
        code_base = info.code_base;
        data_base = info.data_base;

        code_size = info.code_size;
        data_size = info.data_size;
        bss_size = info.bss_size;

        exception_descriptor = info.exception_descriptor;

        sinfo = std::move(info.sinfo);

        ep = info.entry_point;

        export_table = std::move(info.export_table);
        full_path = std::move(info.full_path);

        // Only allows write when it's not open yet
        memory_system *mem = kern->get_memory_system();
        const auto code_size_align = common::align(code_size, mem->get_page_size());
        const auto data_size_align = common::align(data_size + bss_size, mem->get_page_size());

        code_addr = info.code_load_addr;
        data_addr = info.data_load_addr;

        if (code_addr == 0) {
            code_chunk = kern->create<kernel::chunk>(mem, nullptr, name, 0, code_size_align, code_size_align, prot::read_write_exec, kernel::chunk_type::normal,
                kernel::chunk_access::code, kernel::chunk_attrib::none, false);

            code_addr = code_chunk->base().ptr_address();
            info.code_load_addr = code_addr;

            for (auto &export_entry : export_table) {
                export_entry += code_addr - info.code_base;
            }

            ep += code_addr;

            if (exception_descriptor != 0) {
                exception_descriptor += code_addr;
            }
        }

        if (data_size_align && data_addr == 0) {
            data_chunk = kern->create<kernel::chunk>(mem, kern->crr_process(), name, 0, data_size_align, data_size_align, prot::read_write, kernel::chunk_type::normal,
                kernel::chunk_access::code, kernel::chunk_attrib::none, false);

            data_addr = data_chunk->base().ptr_address();

            // BSS is after static data
            std::uint8_t *bss = data_chunk->base().get(kern->get_memory_system()) + data_size;

            // Initialize bss
            // Definitely, there maybe bss, fill that with zero
            // I usually depends on this to get my integer zero
            // Filling zero from beginning of code segment, with size of bss size - 1
            std::fill(bss, bss + bss_size, 0);

            info.data_load_addr = data_addr;
        }
    }

    address codeseg::lookup(const std::uint32_t ord) {
        if (ord > export_table.size()) {
            return 0;
        }

        return export_table[ord - 1];
    }

    void codeseg::queries_call_list(std::vector<std::uint32_t> &call_list) {
        if (mark) {
            return;
        }

        // Iterate through dependency first
        for (auto &dependency : dependencies) {
            if (!dependency->mark) {
                dependency->mark = true;
                dependency->queries_call_list(call_list);
                dependency->mark = false;
            }
        }

        // Add our last. Don't change order, this is how it supposed to be
        call_list.push_back(ep);
    }

    bool codeseg::add_dependency(codeseg_ptr codeseg) {
        // Check if this codeseg is unique first (no duplicate)
        // We don't check the UID though (TODO)
        auto result = std::find_if(dependencies.begin(), dependencies.end(),
            [&](const codeseg_ptr &codeseg_ite) { return codeseg_ite->unique_id() == codeseg->unique_id(); });

        if (result == dependencies.end()) {
            dependencies.push_back(std::move(codeseg));
            return true;
        }

        return false;
    }
}
