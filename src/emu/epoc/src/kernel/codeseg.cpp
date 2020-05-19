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
#include <common/algorithm.h>
#include <epoc/kernel.h>
#include <epoc/kernel/codeseg.h>

namespace eka2l1::kernel {
    codeseg::codeseg(kernel_system *kern, const std::string &name, codeseg_create_info &info)
        : kernel_obj(kern, name, nullptr, kernel::access_type::global_access) {
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

        code_addr = info.code_load_addr;
        data_addr = info.data_load_addr;

        if (info.data_size) {
            constant_data = std::make_unique<std::uint8_t[]>(info.data_size);
            std::copy(info.constant_data, info.constant_data + info.data_size, constant_data.get());
        }

        if (code_addr == 0) {
            code_data = std::make_unique<std::uint8_t[]>(info.code_size);
            std::copy(info.code_data, info.code_data + info.code_size, code_data.get());
        }
    }

    bool codeseg::attach(kernel::process *new_foe) {
        if (new_foe == nullptr) {
            return true;
        }

        if (common::find_and_ret_if(attaches, [=](const attached_info &info) {
                return info.attached_process == new_foe;
            })) {
            return false;
        }

        // Allocate new data chunk for this!
        memory_system *mem = kern->get_memory_system();
        const auto data_size_align = common::align(data_addr ? bss_size : data_size + bss_size, mem->get_page_size());
        const auto code_size_align = common::align(code_size, mem->get_page_size());

        chunk_ptr code_chunk = nullptr;
        chunk_ptr dt_chunk = nullptr;

        if (code_addr == 0) {
            code_chunk = kern->create<kernel::chunk>(mem, nullptr, "", 0, code_size_align, code_size_align, prot::read_write_exec, kernel::chunk_type::normal,
                kernel::chunk_access::code, kernel::chunk_attrib::none, false);

            // Copy data
            std::uint8_t *code_base = reinterpret_cast<std::uint8_t *>(code_chunk->host_base());
            std::copy(code_data.get(), code_data.get() + code_size, code_base); // .code
        }

        if (data_size_align != 0) {
            dt_chunk = kern->create<kernel::chunk>(mem, new_foe, "", 0, data_size_align, data_size_align,
                prot::read_write, kernel::chunk_type::normal, kernel::chunk_access::local, kernel::chunk_attrib::none, false,
                data_addr ? data_base : 0);

            if (!dt_chunk) {
                return false;
            }

            std::uint8_t *dt_base = reinterpret_cast<std::uint8_t *>(dt_chunk->host_base());

            if (data_addr == 0) {
                // Confirmed that if data is in ROM, only BSS is reserved
                std::copy(constant_data.get(), constant_data.get() + data_size, dt_base); // .data
            }

            const std::uint32_t bss_off = data_addr ? 0 : data_size;
            std::fill(dt_base + bss_off, dt_base + bss_off + bss_size, 0); // .bss
        }

        attaches.push_back({ new_foe, dt_chunk, code_chunk });

        return true;
    }

    bool codeseg::detach(kernel::process *de_foe) {
        auto attach_info = common::find_and_ret_if(attaches, [=](const attached_info &info) {
            return info.attached_process == de_foe;
        });

        if (attach_info == nullptr) {
            return false;
        }

        // Free the chunk data
        if (attach_info->data_chunk) {
            kern->destroy(attach_info->data_chunk);
        }

        if (attach_info->code_chunk) {
            kern->destroy(attach_info->code_chunk);
        }

        attaches.erase(attaches.begin() + std::distance(attaches.data(), attach_info));

        if (attaches.empty()) {
            // MUDA MUDA MUDA MUDA MUDA MUDA MUDA
            kern->destroy(kern->get_by_id<kernel::codeseg>(uid));
        }

        return true;
    }

    address codeseg::get_code_run_addr(kernel::process *pr, std::uint8_t **base) {
        if (code_addr != 0) {
            if (base) {
                *base = reinterpret_cast<std::uint8_t *>(kern->get_memory_system()->get_real_pointer(code_addr));
            }

            return code_addr;
        }

        // Find our stuffs
        auto attach_info = common::find_and_ret_if(attaches, [=](const attached_info &info) {
            return info.attached_process == pr;
        });

        if (attach_info == nullptr) {
            return 0;
        }

        if (base) {
            *base = reinterpret_cast<std::uint8_t *>(attach_info->code_chunk->host_base());
        }

        return attach_info->code_chunk->base().ptr_address();
    }

    address codeseg::get_data_run_addr(kernel::process *pr, std::uint8_t **base) {
        if ((data_size == 0) && (bss_size == 0)) {
            return 0;
        }

        if (data_addr != 0) {
            if (base) {
                *base = reinterpret_cast<std::uint8_t *>(kern->get_memory_system()->get_real_pointer(data_addr));
            }

            return data_addr;
        }

        // Find our stuffs
        auto attach_info = common::find_and_ret_if(attaches, [=](const attached_info &info) {
            return info.attached_process == pr;
        });

        if (attach_info == nullptr || !attach_info->data_chunk) {
            return 0;
        }

        if (base) {
            *base = reinterpret_cast<std::uint8_t *>(attach_info->data_chunk->host_base());
        }

        return attach_info->data_chunk->base().ptr_address();
    }

    std::uint32_t codeseg::get_exception_descriptor(kernel::process *pr) {
        // Find our stuffs
        auto attach_info = common::find_and_ret_if(attaches, [=](const attached_info &info) {
            return info.attached_process == pr;
        });

        if (attach_info == nullptr) {
            return 0;
        }

        return attach_info->code_chunk->base().ptr_address() + exception_descriptor;
    }

    address codeseg::get_entry_point(kernel::process *pr) {
        if (code_addr != 0) {
            return ep;
        }

        // Find our stuffs
        auto attach_info = common::find_and_ret_if(attaches, [=](const attached_info &info) {
            return info.attached_process == pr;
        });

        if (attach_info == nullptr) {
            return 0;
        }

        return attach_info->code_chunk->base().ptr_address() + ep;
    }

    address codeseg::lookup_no_relocate(const std::uint32_t ord) {
        if (ord > export_table.size() || ord == 0) {
            return 0;
        }

        return export_table[ord - 1];
    }

    address codeseg::lookup(kernel::process *pr, const std::uint32_t ord) {
        const address lookup_res = lookup_no_relocate(ord);

        if (code_addr != 0 || !lookup_res) {
            return lookup_res;
        }

        auto attach_info = common::find_and_ret_if(attaches, [=](const attached_info &info) {
            return info.attached_process == pr;
        });

        if (attach_info == nullptr) {
            return 0;
        }

        return attach_info->code_chunk->base().ptr_address() + lookup_res - code_base;
    }

    void codeseg::queries_call_list(kernel::process *pr, std::vector<std::uint32_t> &call_list) {
        // Iterate through dependency first
        for (auto &dependency : dependencies) {
            if (!dependency->mark) {
                dependency->mark = true;
                dependency->queries_call_list(pr, call_list);
            }
        }

        // Add our last. Don't change order, this is how it supposed to be
        call_list.push_back(get_entry_point(pr));
    }

    void codeseg::unmark() {
        for (auto &dependency : dependencies) {
            if (dependency->mark) {
                dependency->mark = false;
                dependency->unmark();
            }
        }
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

    std::vector<std::uint32_t> codeseg::get_export_table(kernel::process *pr) {
        if (code_addr != 0) {
            return export_table;
        }

        auto attach_info = common::find_and_ret_if(attaches, [=](const attached_info &info) {
            return info.attached_process == pr;
        });

        if (attach_info == nullptr) {
            return {};
        }

        std::vector<std::uint32_t> new_table = export_table;
        const std::uint32_t delta = attach_info->code_chunk->base().ptr_address() - code_base;

        for (auto &entry : new_table) {
            entry += delta;
        }

        return new_table;
    }

    void codeseg::set_export(const std::uint32_t ordinal, eka2l1::ptr<void> address) {
        if (export_table.size() < ordinal) {
            return;
        }

        export_table[ordinal - 1] = address.ptr_address();
    }
}
