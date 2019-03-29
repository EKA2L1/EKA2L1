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

#include <scripting/instance.h>
#include <scripting/codeseg.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>
#include <epoc/kernel/codeseg.h>
#include <epoc/kernel/libmanager.h>

#include <common/cvt.h>
#include <pybind11/embed.h>

namespace eka2l1::scripting {    
    codeseg::codeseg(std::uint64_t handle)
        : real_seg_(reinterpret_cast<kernel::codeseg*>(handle)) {
    }

    std::uint32_t codeseg::lookup(const std::uint32_t ord) {
        return real_seg_->lookup(ord);
    }

    std::uint32_t codeseg::code_run_address() {
        return real_seg_->get_code_run_addr();
    }

    std::uint32_t codeseg::data_run_address() {
        return real_seg_->get_data_run_addr();
    }

    std::uint32_t codeseg::bss_run_address() {
        return real_seg_->get_data_run_addr() + real_seg_->get_data_size();
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
        return static_cast<std::uint32_t>(real_seg_->get_export_table().size());
    }

    std::unique_ptr<scripting::codeseg> load_codeseg(const std::string &virt_path) {
        eka2l1::system *sys =  get_current_instance();
        hle::lib_manager *libmngr = sys->get_lib_manager();

        codeseg_ptr seg = libmngr->load(common::utf8_to_ucs2(virt_path));

        if (!seg) {
            throw std::runtime_error("Loading codeseg failed! Does the codeseg file exist, or it's valid?");
            return nullptr;
        }

        return std::make_unique<scripting::codeseg>(reinterpret_cast<std::uint64_t>(&(*seg)));
    }
}
