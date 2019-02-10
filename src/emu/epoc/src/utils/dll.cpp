/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <epoc/epoc.h>
#include <epoc/kernel.h>

#include <epoc/utils/dll.h>
#include <epoc/kernel/libmanager.h>
#include <epoc/kernel/codeseg.h>

namespace eka2l1::epoc {
    std::optional<std::u16string> get_dll_full_path(eka2l1::system *sys, const std::uint32_t addr) {
        hle::lib_manager &mngr = *sys->get_lib_manager();

        for (const auto &[id, seg] : mngr.cached_segs) {
            if (seg->get_entry_point() == addr) {
                return seg->get_full_path();
            }
        }

        return std::optional<std::u16string>{};
    }

    address get_exception_descriptor_addr(eka2l1::system *sys, address runtime_addr) {
        hle::lib_manager *manager = sys->get_lib_manager();

        for (const auto &[id, seg] : manager->cached_segs) {
            if (seg->get_code_run_addr() <= runtime_addr && seg->get_code_run_addr() + seg->get_code_size() >= runtime_addr) {
                return seg->get_exception_descriptor();
            }
        }

        return 0;
    }
}