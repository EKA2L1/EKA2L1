/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <scripting/codeseg.h>
#include <scripting/instance.h>
#include <scripting/process.h>

#include <kernel/codeseg.h>
#include <kernel/kernel.h>
#include <kernel/libmanager.h>
#include <system/epoc.h>

#include <common/cvt.h>

namespace eka2l1::scripting {
    codeseg::codeseg(std::uint64_t handle)
        : real_seg_(reinterpret_cast<kernel::codeseg *>(handle)) {
        real_seg_->increase_access_count();
    }

    codeseg::~codeseg() {
        real_seg_->decrease_access_count();
    }

    std::uint32_t codeseg::lookup(process_inst pr, const std::uint32_t ord) {
        return real_seg_->lookup(pr ? pr->get_process_handle() : nullptr, ord);
    }

    std::uint32_t codeseg::code_run_address(process_inst pr) {
        return real_seg_->get_code_run_addr(pr ? pr->get_process_handle() : nullptr);
    }

    std::uint32_t codeseg::data_run_address(process_inst pr) {
        return real_seg_->get_data_run_addr(pr ? pr->get_process_handle() : nullptr);
    }

    std::uint32_t codeseg::bss_run_address(process_inst pr) {
        return real_seg_->get_data_run_addr(pr ? pr->get_process_handle() : nullptr) + real_seg_->get_data_size();
    }

    std::uint32_t codeseg::code_size() {
        return real_seg_->get_code_size();
    }

    std::uint32_t codeseg::data_size() {
        return real_seg_->get_data_size();
    }

    std::uint32_t codeseg::bss_size() {
        return real_seg_->get_bss_size();
    }

    std::uint32_t codeseg::get_export_count() {
        return real_seg_->export_count();
    }

    void codeseg::set_export(const std::uint32_t index, const std::uint32_t value) {
        real_seg_->set_export(index, value);
    }

    void codeseg::set_patched() {
        real_seg_->set_patched();
    }
    
    void codeseg::set_entry_point(const std::uint32_t value) {
        real_seg_->set_entry_point(value);
    }

    void codeseg::set_entry_point_disabled() {
        real_seg_->set_entry_point_disabled();
    }

    std::uint32_t codeseg::get_export(process *pr, const std::uint32_t index) {
        std::vector<std::uint32_t> table = real_seg_->get_export_table(pr ? pr->get_process_handle() : nullptr);
        if (table.size() < index) {
            return 0xFFFFFFFF;
        }
        return table[index - 1];
    }

    std::uint32_t codeseg::get_entry_point(process *pr) {
        return real_seg_->get_entry_point(pr ? pr->get_process_handle() : nullptr);
    }
}

extern "C" {
EKA2L1_EXPORT void eka2l1_free_codeseg(eka2l1::scripting::codeseg *seg) {
    delete seg;
}

EKA2L1_EXPORT eka2l1::scripting::codeseg *eka2l1_load_codeseg(const char *path) {
    eka2l1::system *sys = eka2l1::scripting::get_current_instance();
    eka2l1::hle::lib_manager *libmngr = sys->get_lib_manager();

    eka2l1::kernel::codeseg_ptr seg = libmngr->load(eka2l1::common::utf8_to_ucs2(path));

    if (!seg) {
        return nullptr;
    }

    return new eka2l1::scripting::codeseg(reinterpret_cast<std::uint64_t>(seg));
}

EKA2L1_EXPORT std::uint32_t eka2l1_codeseg_lookup(eka2l1::scripting::codeseg *seg, eka2l1::scripting::process *pr,
    const std::uint32_t ord) {
    return seg->real_seg_->lookup(pr ? pr->get_process_handle() : nullptr, ord);
}

EKA2L1_EXPORT std::uint32_t eka2l1_codeseg_code_run_address(eka2l1::scripting::codeseg *seg, eka2l1::scripting::process *pr) {
    return seg->real_seg_->get_code_run_addr(pr ? pr->get_process_handle() : nullptr);
}

EKA2L1_EXPORT std::uint32_t eka2l1_codeseg_data_run_address(eka2l1::scripting::codeseg *seg, eka2l1::scripting::process *pr) {
    return seg->real_seg_->get_data_run_addr(pr ? pr->get_process_handle() : nullptr);
}

EKA2L1_EXPORT std::uint32_t eka2l1_codeseg_bss_run_address(eka2l1::scripting::codeseg *seg, eka2l1::scripting::process *pr) {
    return seg->real_seg_->get_data_run_addr(pr ? pr->get_process_handle() : nullptr) + seg->real_seg_->get_data_size();
}

EKA2L1_EXPORT std::uint32_t eka2l1_codeseg_code_size(eka2l1::scripting::codeseg *seg) {
    return seg->code_size();
}

EKA2L1_EXPORT std::uint32_t eka2l1_codeseg_data_size(eka2l1::scripting::codeseg *seg) {
    return seg->data_size();
}

EKA2L1_EXPORT std::uint32_t eka2l1_codeseg_bss_size(eka2l1::scripting::codeseg *seg) {
    return seg->bss_size();
}

EKA2L1_EXPORT std::uint32_t eka2l1_codeseg_export_count(eka2l1::scripting::codeseg *seg) {
    return seg->get_export_count();
}

EKA2L1_EXPORT void eka2l1_codeseg_set_export(eka2l1::scripting::codeseg *seg, const std::uint32_t index, const std::uint32_t addr_or_offset) {
    seg->set_export(index, addr_or_offset);
}

EKA2L1_EXPORT void eka2l1_codeseg_set_entry_point(eka2l1::scripting::codeseg *seg, const std::uint32_t addr_or_offset) {
    seg->set_entry_point(addr_or_offset);
}

EKA2L1_EXPORT void eka2l1_codeseg_set_patched(eka2l1::scripting::codeseg *seg) {
    seg->set_patched();
}

EKA2L1_EXPORT void eka2l1_codeseg_set_entry_point_disabled(eka2l1::scripting::codeseg *seg) {
    seg->set_entry_point_disabled();
}

EKA2L1_EXPORT std::uint32_t eka2l1_codeseg_get_export(eka2l1::scripting::codeseg *seg, eka2l1::scripting::process  *pr, const std::uint32_t index) {
    return seg->get_export(pr, index);
}

EKA2L1_EXPORT std::uint32_t eka2l1_codeseg_get_entry_point(eka2l1::scripting::codeseg *seg, eka2l1::scripting::process  *pr) {
    return seg->get_entry_point(pr);
}
}
