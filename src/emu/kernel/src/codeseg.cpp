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

#include <common/algorithm.h>
#include <common/log.h>
#include <kernel/kernel.h>
#include <kernel/codeseg.h>
#include <loader/common.h>

#include <algorithm>

namespace eka2l1::kernel {
    codeseg::codeseg(kernel_system *kern, const std::string &name, codeseg_create_info &info)
        : kernel_obj(kern, name, nullptr, kernel::access_type::global_access) {
        std::copy(info.uids, info.uids + 3, uids);
        code_base = info.code_base;
        data_base = info.data_base;

        code_size = info.code_size;
        data_size = info.data_size;
        bss_size = info.bss_size;
        text_size = info.text_size;

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

        relocation_list = info.relocation_list;
    }

    bool codeseg::attach(kernel::process *new_foe, const bool forcefully) {
        if (!new_foe && !forcefully) {
            return false;
        }

        if (!forcefully) {
            if (common::find_and_ret_if(attaches, [=](const attached_info &info) {
                    return info.attached_process == new_foe;
                })) {
                return false;
            }
        }

        // Allocate new data chunk for this!
        memory_system *mem = kern->get_memory_system();
        const auto data_size_align = common::align(data_addr ? bss_size : data_size + bss_size, mem->get_page_size());
        const auto code_size_align = common::align(code_size, mem->get_page_size());

        chunk_ptr code_chunk = nullptr;
        chunk_ptr dt_chunk = nullptr;
        
        address the_addr_of_code_run = 0;
        address the_addr_of_data_run = 0;

        std::uint8_t *code_base_ptr = nullptr;
        std::uint8_t *data_base_ptr = nullptr;

        if (code_addr == 0) {
            code_chunk = kern->create<kernel::chunk>(mem, new_foe, "", 0, code_size_align, code_size_align, prot::read_write_exec, kernel::chunk_type::normal,
                kernel::chunk_access::code, kernel::chunk_attrib::none, false);

            the_addr_of_code_run = code_chunk->base(new_foe).ptr_address();

            // Copy data
            code_base_ptr = reinterpret_cast<std::uint8_t *>(code_chunk->host_base());
            std::copy(code_data.get(), code_data.get() + code_size, code_base_ptr); // .code
        } else {
            the_addr_of_code_run = code_addr;
            code_base_ptr = reinterpret_cast<std::uint8_t *>(kern->get_memory_system()->get_real_pointer(code_addr));
        }
        
        // Attach all of its dependencies
        if (!forcefully || (code_addr && forcefully)) {
            for (auto &dependency: dependencies) {
                dependency.dep_->attach(new_foe);

                // Patch what imports we need
                for (const std::uint64_t import: dependency.import_info_) {
                    const std::uint16_t ord = (import & 0xFFFF);
                    const std::uint16_t adj = (import >> 16) & 0xFFFF;
                    const std::uint32_t offset_to_apply = (import >> 32) & 0xFFFFFFFF;

                    const address addr = dependency.dep_->lookup(new_foe, ord);
                    *reinterpret_cast<std::uint32_t*>(&code_base_ptr[offset_to_apply]) = addr + adj;
                }
            }
        }

        if (data_size_align != 0) {
            if (!data_addr) {
                dt_chunk = kern->create<kernel::chunk>(mem, new_foe, "", 0, data_size_align, data_size_align,
                    prot::read_write, kernel::chunk_type::normal, kernel::chunk_access::local, kernel::chunk_attrib::anonymous);
            } else {
                dt_chunk = new_foe->get_rom_bss_chunk(); 
            }

            if (!dt_chunk) {
                return false;
            }

            data_base_ptr = reinterpret_cast<std::uint8_t *>(dt_chunk->host_base());
            the_addr_of_data_run = dt_chunk->base(new_foe).ptr_address();

            if (data_addr) {
                data_base_ptr += (data_base - the_addr_of_data_run);
            }

            // Confirmed that if data is in ROM, only BSS is reserved
            std::copy(constant_data.get(), constant_data.get() + data_size, data_base_ptr); // .data

            const std::uint32_t bss_off = data_size;
            std::fill(data_base_ptr + bss_off, data_base_ptr + bss_off + bss_size, 0); // .bss
        } else {
            the_addr_of_data_run = data_addr;
            data_base_ptr = reinterpret_cast<std::uint8_t *>(kern->get_memory_system()->get_real_pointer(data_addr));
        }
        
        LOG_INFO("{} runtime code: 0x{:x}", name(), the_addr_of_code_run);
        LOG_INFO("{} runtime data: 0x{:x}", name(), the_addr_of_data_run);

        if (!relocation_list.empty()) {
            const std::uint32_t code_delta = the_addr_of_code_run - code_base;
            const std::uint32_t data_delta = the_addr_of_data_run - data_base;

            // Relocate the image
            for (const std::uint64_t relocate_info: relocation_list) {
                const loader::relocation_type rel_type = static_cast<loader::relocation_type>((relocate_info >> 32) & 0xFFFF);
                const loader::relocate_section sect_type = static_cast<loader::relocate_section>((relocate_info >> 48) & 0xFFFF);
                const std::uint32_t offset_to_relocate = static_cast<std::uint32_t>(relocate_info);
                address the_delta = 0;

                switch (rel_type) {
                case loader::relocation_type::data:
                    the_delta = data_delta;
                    break;
                
                case loader::relocation_type::text:
                    the_delta = code_delta;
                    break;

                case loader::relocation_type::inferred:
                    the_delta = (offset_to_relocate < text_size) ? code_delta : data_delta;
                    break;

                case loader::relocation_type::reserved:
                    continue;

                default:
                    LOG_ERROR("Unknown code relocation type {}", static_cast<std::uint32_t>(rel_type));
                    break;
                }

                std::uint8_t *base_ptr = nullptr;

                switch (sect_type) {
                case loader::relocate_section_text:
                    base_ptr = code_base_ptr;
                    break;

                case loader::relocate_section_data:
                    base_ptr = data_base_ptr;
                    break;

                default:
                    break;
                }

                std::uint32_t *to_relocate_ptr = reinterpret_cast<std::uint32_t*>(&base_ptr[offset_to_relocate]);
                *to_relocate_ptr = *to_relocate_ptr + the_delta;
            }
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

        return attach_info->code_chunk->base(pr).ptr_address();
    }

    address codeseg::get_data_run_addr(kernel::process *pr, std::uint8_t **base) {
        if ((data_size == 0) && (bss_size == 0)) {
            return 0;
        }

        if (data_addr != 0) {
            // Intentional, in this case it's ROM
            if (base) {
                *base = reinterpret_cast<std::uint8_t *>(kern->get_memory_system()->get_real_pointer(data_base));
            }

            return data_base;
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

        return attach_info->data_chunk->base(pr).ptr_address();
    }

    std::uint32_t codeseg::get_exception_descriptor(kernel::process *pr) {
        // Find our stuffs
        auto attach_info = common::find_and_ret_if(attaches, [=](const attached_info &info) {
            return info.attached_process == pr;
        });

        if (attach_info == nullptr) {
            return 0;
        }

        return attach_info->code_chunk->base(pr).ptr_address() + exception_descriptor;
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

        return attach_info->code_chunk->base(pr).ptr_address() + ep;
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

        return attach_info->code_chunk->base(pr).ptr_address() + lookup_res - code_base;
    }

    void codeseg::queries_call_list(kernel::process *pr, std::vector<std::uint32_t> &call_list) {
        // Add forced entry points
        call_list.insert(call_list.end(), premade_eps.begin(), premade_eps.end());

        // Iterate through dependency first
        for (auto &dependency : dependencies) {
            if (!dependency.dep_->mark) {
                dependency.dep_->mark = true;
                dependency.dep_->queries_call_list(pr, call_list);
            }
        }

        // Add our last. Don't change order, this is how it supposed to be
        call_list.push_back(get_entry_point(pr));
    }

    void codeseg::unmark() {
        for (auto &dependency : dependencies) {
            if (dependency.dep_->mark) {
                dependency.dep_->mark = false;
                dependency.dep_->unmark();
            }
        }
    }

    bool codeseg::add_dependency(const codeseg_dependency_info &codeseg) {
        // Check if this codeseg is unique first (no duplicate)
        // We don't check the UID though (TODO)
        auto result = std::find_if(dependencies.begin(), dependencies.end(),
            [&](const codeseg_dependency_info &codeseg_ite) { return codeseg_ite.dep_->unique_id() == codeseg.dep_->unique_id(); });

        if (result == dependencies.end()) {
            dependencies.push_back(std::move(codeseg));
            return true;
        }

        LOG_TRACE("Failed");
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
        const std::uint32_t delta = attach_info->code_chunk->base(pr).ptr_address() - code_base;

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

    bool codeseg::add_premade_entry_point(const address addr) {
        if (std::find(premade_eps.begin(), premade_eps.end(), addr) == premade_eps.end()) {
            premade_eps.push_back(addr);
            return true;
        }

        return false;
    }

    address codeseg::relocate(kernel::process *pr, const address addr_on_base) {
        return addr_on_base - get_code_base() + get_code_run_addr(pr, nullptr);
    }
}
