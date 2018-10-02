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

#include <core/epoc/dll.h>
#include <core/core.h>

namespace eka2l1::epoc {
    std::vector<uint32_t> query_entries(eka2l1::system *sys) {
        hle::lib_manager &mngr = *sys->get_lib_manager();
        std::vector<uint32_t> entries;
        
        auto cache_maps = mngr.get_e32imgs_cache();
        address exe_addr = 0;

        for (auto &cache : cache_maps) {
            auto res = std::find(cache.second.loader.begin(), cache.second.loader.end(), sys->get_kernel_system()->crr_process());

            // If this image is owned to the current process
            if (res != cache.second.loader.end()) {
                if (cache.second.img->header.uid1 != loader::eka2_img_type::exe) {
                    entries.push_back(cache.second.img->header.entry_point + cache.second.img->rt_code_addr);
                }
            }
        }

        entries.push_back(0x2);

        return entries;
    }
}