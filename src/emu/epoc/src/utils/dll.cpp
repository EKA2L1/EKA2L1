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

#include <epoc/loader/e32img.h>
#include <epoc/loader/romimage.h>

#include <epoc/kernel/libmanager.h>

namespace eka2l1::epoc {
    std::vector<uint32_t> query_entries(eka2l1::system *sys) {
        hle::lib_manager &mngr = *(sys->get_lib_manager());
        std::vector<uint32_t> entries;

        auto cache_maps = mngr.get_e32imgs_cache();
        auto rom_cache_maps = mngr.get_romimgs_cache();

        address exe_addr = 0;

        process_ptr pr = sys->get_kernel_system()->crr_process();
        std::uint32_t uid = pr->get_uid();

        for (auto &cache : rom_cache_maps) {
            auto res = std::find(cache.second.loader.begin(), cache.second.loader.end(), pr);

            // If this image is owned to the current process
            if (res != cache.second.loader.end()) {
                if (cache.second.img->header.uid3 != uid) {
                    entries.push_back(cache.second.img->header.entry_point);
                } else {
                    exe_addr = cache.second.img->header.entry_point;
                }
            }
        }

        for (auto &cache : cache_maps) {
            auto res = std::find(cache.second.loader.begin(), 
                cache.second.loader.end(), pr);

            // If this image is owned to the current process
            if (res != cache.second.loader.end()) {
                if (cache.second.img->header.uid1 != loader::e32_img_type::exe) {
                    entries.push_back(cache.second.img->header.entry_point + cache.second.img->rt_code_addr);
                }
                else {
                    exe_addr = cache.second.img->header.entry_point + cache.second.img->rt_code_addr;
                }
            }
        }

        entries.push_back(exe_addr);

        return entries;
    }

    std::optional<std::u16string> get_dll_full_path(eka2l1::system *sys, const std::uint32_t addr) {
        hle::lib_manager &mngr = *sys->get_lib_manager();

        for (const auto & [ id, imgwr ] : mngr.get_romimgs_cache()) {
            if (imgwr.img->header.entry_point == addr) {
                return imgwr.full_path;
            }
        }
        
        for (const auto & [ id, imgwr ] : mngr.get_e32imgs_cache()) {
            if (imgwr.img->header.entry_point + imgwr.img->rt_code_addr == addr) {
                return imgwr.full_path;
            }
        }

        return std::optional<std::u16string>{};
    }
    
    address get_exception_descriptor_addr(eka2l1::system *sys, address runtime_addr) {
        hle::lib_manager *manager = sys->get_lib_manager();
        auto &e32map = manager->get_e32imgs_cache();

        for (const auto &e32imginf: e32map) {
            const loader::e32img_ptr &e32img = e32imginf.second.img;

            if (e32img->rt_code_addr <= runtime_addr && e32img->rt_code_addr + e32img->header.code_size >= runtime_addr) {
                if (e32img->has_extended_header && (e32img->header_extended.exception_des & 1)) {
                    return e32img->rt_code_addr + e32img->header_extended.exception_des - 1;
                }
                
                return 0;
            }
        }

        // Look for ROM exception descriptor
        auto &rommap = manager->get_romimgs_cache();
        for (const auto &romimginf: rommap) {
            const loader::romimg_ptr &romimg = romimginf.second.img;

            if (romimg->header.code_address <= runtime_addr && romimg->header.code_address + romimg->header.code_size >= runtime_addr) {
                return romimg->header.exception_des;
            }
        }

        return 0;
    }
}